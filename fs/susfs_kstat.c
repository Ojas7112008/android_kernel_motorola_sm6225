// SPDX-License-Identifier: GPL-2.0
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/overflow.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/susfs.h>
#include <linux/susfs_meta.h>
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
#include <linux/susfs_mount.h>
#endif
#include <linux/uaccess.h>
#include "mount.h"

/* ARM EABI userspace longs are 32 bits; long long fields retain 8-byte alignment. */
struct susfs_kstat_compat {
	u8 is_statically;
	u32 target_ino;
	char target_pathname[SUSFS_MAX_LEN_PATHNAME];
	u32 spoofed_ino;
	u32 spoofed_dev;
	u32 spoofed_nlink;
	s64 spoofed_size __aligned(8);
	s32 spoofed_atime_tv_sec;
	u32 spoofed_atime_tv_nsec;
	s32 spoofed_mtime_tv_sec;
	u32 spoofed_mtime_tv_nsec;
	s32 spoofed_ctime_tv_sec;
	u32 spoofed_ctime_tv_nsec;
	s64 spoofed_blocks __aligned(8);
	s32 spoofed_blksize;
	int flags;
	int err;
};

struct susfs_kstat_rule {
	struct hlist_node node;
	struct rcu_head rcu;
	unsigned long ino;
	dev_t dev;
	struct st_susfs_sus_kstat info;
	struct kstatfs statfs;
	int mnt_id;
};

static DEFINE_MUTEX(susfs_kstat_lock);
static DEFINE_HASHTABLE(susfs_kstat_rules, 8);

static int susfs_copy_kstat_request(void __user **arg,
				    struct st_susfs_sus_kstat *info)
{
	if (!arg || !*arg)
		return -EFAULT;
	if (in_compat_syscall()) {
		struct susfs_kstat_compat compat;

		if (copy_from_user(&compat, *arg, sizeof(compat)))
			return -EFAULT;
		if (compat.is_statically > 1)
			return -EINVAL;
#define COPY_KSTAT_FIELD(field) info->field = compat.field
		COPY_KSTAT_FIELD(is_statically);
		COPY_KSTAT_FIELD(target_ino);
		memcpy(info->target_pathname, compat.target_pathname,
		       sizeof(info->target_pathname));
		COPY_KSTAT_FIELD(spoofed_ino);
		COPY_KSTAT_FIELD(spoofed_dev);
		COPY_KSTAT_FIELD(spoofed_nlink);
		COPY_KSTAT_FIELD(spoofed_size);
		COPY_KSTAT_FIELD(spoofed_atime_tv_sec);
		COPY_KSTAT_FIELD(spoofed_atime_tv_nsec);
		COPY_KSTAT_FIELD(spoofed_mtime_tv_sec);
		COPY_KSTAT_FIELD(spoofed_mtime_tv_nsec);
		COPY_KSTAT_FIELD(spoofed_ctime_tv_sec);
		COPY_KSTAT_FIELD(spoofed_ctime_tv_nsec);
		COPY_KSTAT_FIELD(spoofed_blocks);
		COPY_KSTAT_FIELD(spoofed_blksize);
		COPY_KSTAT_FIELD(flags);
#undef COPY_KSTAT_FIELD
	} else {
		if (copy_from_user(info, *arg, sizeof(*info)))
			return -EFAULT;
		/* Validate the representation before evaluating a userspace bool. */
		if (*(u8 *)&info->is_statically > 1)
			return -EINVAL;
	}
	if (!info->target_pathname[0] ||
	    strnlen(info->target_pathname, sizeof(info->target_pathname)) ==
						sizeof(info->target_pathname) ||
	    (info->flags & ~0xfff))
		return -EINVAL;
	if (((info->flags & KSTAT_SPOOF_ATIME_TV_NSEC) &&
	     info->spoofed_atime_tv_nsec >= NSEC_PER_SEC) ||
	    ((info->flags & KSTAT_SPOOF_MTIME_TV_NSEC) &&
	     info->spoofed_mtime_tv_nsec >= NSEC_PER_SEC) ||
	    ((info->flags & KSTAT_SPOOF_CTIME_TV_NSEC) &&
	     info->spoofed_ctime_tv_nsec >= NSEC_PER_SEC) ||
	    ((info->flags & KSTAT_SPOOF_SIZE) && info->spoofed_size < 0) ||
	    ((info->flags & KSTAT_SPOOF_BLOCKS) && info->spoofed_blocks < 0) ||
	    ((info->flags & KSTAT_SPOOF_BLKSIZE) &&
	     (info->spoofed_blksize < 0 || info->spoofed_blksize > 0xffffffffUL)))
		return -EINVAL;
	return 0;
}

static void susfs_reply_kstat(void __user **arg, int err)
{
	size_t offset = in_compat_syscall() ?
		offsetof(struct susfs_kstat_compat, err) :
		offsetof(struct st_susfs_sus_kstat, err);
	unsigned long address;

	if (arg && *arg &&
	    !check_add_overflow((unsigned long)*arg, (unsigned long)offset, &address))
		(void)copy_to_user((void __user *)address, &err, sizeof(err));
}

static void susfs_store_kstat(void __user **arg, bool update)
{
	struct st_susfs_sus_kstat request = { 0 };
	struct susfs_kstat_rule *rule, *old, *entry;
	struct hlist_node *tmp;
	struct path path;
	struct inode *inode;
	unsigned int bucket;
	int err;

	err = susfs_copy_kstat_request(arg, &request);
	if (err)
		goto reply;
	rule = kzalloc(sizeof(*rule), GFP_KERNEL);
	if (!rule) {
		err = -ENOMEM;
		goto reply;
	}
	err = kern_path(request.target_pathname, 0, &path);
	if (err)
		goto free_rule;
	inode = d_backing_inode(path.dentry);
	err = -ENOENT;
	if (!inode || !inode->i_mapping)
		goto put_path;

	mutex_lock(&susfs_kstat_lock);
	old = NULL;
	hash_for_each(susfs_kstat_rules, bucket, entry, node) {
		if (!strcmp(entry->info.target_pathname, request.target_pathname)) {
			old = entry;
			break;
		}
	}
	if (update) {
		if (!old)
			goto unlock;
		rule->info = old->info;
		/* The tool distinguishes normal and full-clone updates by flags. */
		rule->info.flags = request.flags;
		rule->statfs = old->statfs;
		rule->mnt_id = old->mnt_id;
	} else {
		rule->info = request;
		rule->info.spoofed_dev = huge_decode_dev(request.spoofed_dev);
		err = susfs_statfs_snapshot(&path, &rule->statfs);
		if (err)
			goto unlock;
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
		rule->mnt_id = susfs_mnt_nonhidden_id(path.mnt);
#else
		rule->mnt_id = real_mount(path.mnt)->mnt_id;
#endif
	}
	/* Resolve native identity, including FUSE's private inode, from the path. */
	rule->ino = inode->i_ino;
	rule->dev = inode->i_sb->s_dev;
	rule->info.target_ino = rule->ino;
	/* Never pin an inode/mount or dereference an inode after path_put(). */
	set_bit(AS_FLAGS_SUS_KSTAT, &inode->i_mapping->flags);
	if (old) {
		hash_del_rcu(&old->node);
		kfree_rcu(old, rcu);
	}
	/* A native identity has one authoritative rule, even through path aliases. */
	hash_for_each_possible_safe(susfs_kstat_rules, entry, tmp, node, rule->ino) {
		if (entry->ino == rule->ino && entry->dev == rule->dev) {
			hash_del_rcu(&entry->node);
			kfree_rcu(entry, rcu);
		}
	}
	hash_add_rcu(susfs_kstat_rules, &rule->node, rule->ino);
	mutex_unlock(&susfs_kstat_lock);
	path_put(&path);
	err = 0;
	goto reply;
unlock:
	mutex_unlock(&susfs_kstat_lock);
put_path:
	path_put(&path);
free_rule:
	kfree(rule);
reply:
	susfs_reply_kstat(arg, err);
}

void susfs_add_sus_kstat(void __user **arg)
{
	susfs_store_kstat(arg, false);
}

void susfs_update_sus_kstat(void __user **arg)
{
	susfs_store_kstat(arg, true);
}

/* The caller holds RCU; the mapping marker prevents stale rules on inode reuse. */
static struct susfs_kstat_rule *susfs_find_kstat(struct inode *inode)
{
	struct susfs_kstat_rule *rule;

	if (!susfs_is_current_app_uid() || !inode || !inode->i_mapping ||
	    !test_bit(AS_FLAGS_SUS_KSTAT, &inode->i_mapping->flags))
		return NULL;
	hash_for_each_possible_rcu(susfs_kstat_rules, rule, node, inode->i_ino)
		if (rule->ino == inode->i_ino && rule->dev == inode->i_sb->s_dev)
			return rule;
	return NULL;
}

void susfs_spoof_kstat(struct inode *inode, struct kstat *stat, u64 *mnt_id)
{
	struct susfs_kstat_rule *rule;
	const struct st_susfs_sus_kstat *info;

	rcu_read_lock();
	rule = susfs_find_kstat(inode);
	if (!rule)
		goto unlock;
	info = &rule->info;
#define SPOOF_KSTAT(flag, field, value) do { \
	if (info->flags & (flag)) \
		stat->field = info->value; \
} while (0)
	SPOOF_KSTAT(KSTAT_SPOOF_INO, ino, spoofed_ino);
	SPOOF_KSTAT(KSTAT_SPOOF_DEV, dev, spoofed_dev);
	SPOOF_KSTAT(KSTAT_SPOOF_NLINK, nlink, spoofed_nlink);
	SPOOF_KSTAT(KSTAT_SPOOF_SIZE, size, spoofed_size);
	SPOOF_KSTAT(KSTAT_SPOOF_ATIME_TV_SEC, atime.tv_sec, spoofed_atime_tv_sec);
	SPOOF_KSTAT(KSTAT_SPOOF_ATIME_TV_NSEC, atime.tv_nsec, spoofed_atime_tv_nsec);
	SPOOF_KSTAT(KSTAT_SPOOF_MTIME_TV_SEC, mtime.tv_sec, spoofed_mtime_tv_sec);
	SPOOF_KSTAT(KSTAT_SPOOF_MTIME_TV_NSEC, mtime.tv_nsec, spoofed_mtime_tv_nsec);
	SPOOF_KSTAT(KSTAT_SPOOF_CTIME_TV_SEC, ctime.tv_sec, spoofed_ctime_tv_sec);
	SPOOF_KSTAT(KSTAT_SPOOF_CTIME_TV_NSEC, ctime.tv_nsec, spoofed_ctime_tv_nsec);
	SPOOF_KSTAT(KSTAT_SPOOF_BLOCKS, blocks, spoofed_blocks);
	SPOOF_KSTAT(KSTAT_SPOOF_BLKSIZE, blksize, spoofed_blksize);
#undef SPOOF_KSTAT
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	if (mnt_id)
		*mnt_id = rule->mnt_id;
#endif
unlock:
	rcu_read_unlock();
}

bool susfs_spoof_statfs(struct inode *inode, struct kstatfs *stat)
{
	struct susfs_kstat_rule *rule;
	bool found = false;

	rcu_read_lock();
	rule = susfs_find_kstat(inode);
	if (rule) {
		*stat = rule->statfs;
		found = true;
	}
	rcu_read_unlock();
	return found;
}

bool susfs_spoof_inode_ids(struct inode *inode, dev_t *dev,
			   unsigned long *ino, int *mnt_id)
{
	struct susfs_kstat_rule *rule;
	bool found = false;

	rcu_read_lock();
	rule = susfs_find_kstat(inode);
	if (rule) {
		if (dev && (rule->info.flags & KSTAT_SPOOF_DEV))
			*dev = rule->info.spoofed_dev;
		if (ino && (rule->info.flags & KSTAT_SPOOF_INO))
			*ino = rule->info.spoofed_ino;
		if (mnt_id)
			*mnt_id = rule->mnt_id;
		found = true;
	}
	rcu_read_unlock();
	return found;
}
