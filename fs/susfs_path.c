// SPDX-License-Identifier: GPL-2.0
/* SUSFS 2.3.0 path core for devon; lookup/readdir consumers are still gated. */
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/init_task.h>
#include <linux/ksu_susfs.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/nsproxy.h>
#include <linux/overflow.h>
#include <linux/printk.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/srcu.h>
#include <linux/string.h>
#include <linux/susfs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#if !defined(CONFIG_KSU_SUSFS_SUS_PATH) || !defined(CONFIG_SRCU)
#error "devon SUSFS path core requires SUS_PATH and SRCU"
#endif

extern struct cred *ksu_cred;

/* Entries are immutable after publication and live until reboot (built-in). */
struct susfs_path_entry {
	struct list_head node;
	char pathname[SUSFS_MAX_LEN_PATHNAME];
};
#define SUSFS_PATH_LOOP_MAX_ENTRIES 1024
static LIST_HEAD(susfs_path_entries);
static DEFINE_MUTEX(susfs_path_lock);
DEFINE_STATIC_SRCU(susfs_path_srcu);
static unsigned int susfs_path_entry_count;

const struct qstr susfs_fake_qstr_name = QSTR_INIT("..5.u.S", 7);

static void susfs_path_reply(void __user **user_info, int err)
{
	unsigned long address;

	if (!user_info || !*user_info ||
	    check_add_overflow((unsigned long)*user_info,
			       (unsigned long)offsetof(struct st_susfs_sus_path, err),
			       &address))
		return;
	if (copy_to_user((void __user *)address, &err, sizeof(err)))
		pr_debug("susfs: failed to return path command error\n");
}

static int susfs_path_request(void __user **user_info,
			      struct st_susfs_sus_path *info)
{
	if (!user_info || copy_from_user(info, *user_info, sizeof(*info)))
		return -EFAULT;
	if (!memchr(info->target_pathname, '\0', sizeof(info->target_pathname)))
		return -ENAMETOOLONG;
	if (!info->target_pathname[0])
		return -EINVAL;
	return 0;
}

static int susfs_path_mark(const char *pathname)
{
	struct path path;
	struct inode *inode;
	int ret;

	ret = kern_path(pathname, LOOKUP_FOLLOW, &path);
	if (ret)
		return ret;
	inode = d_backing_inode(path.dentry);
	if (!inode || !inode->i_mapping) {
		ret = -ENOENT;
	} else {
		/* FUSE embeds this same VFS inode; do not cast unrelated inodes. */
		set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
		ret = 0;
	}
	path_put(&path);
	return ret;
}

void susfs_add_sus_path(void __user **user_info)
{
	struct st_susfs_sus_path info = { 0 };
	int ret;

	ret = susfs_path_request(user_info, &info);
	if (!ret)
		/* Preserve caller permissions and its current namespace for this call. */
		ret = susfs_path_mark(info.target_pathname);
	susfs_path_reply(user_info, ret);
}

/* Deferred names belong to the initial mount namespace and its full root. */
static bool susfs_path_loop_context(void)
{
	struct path caller_root, initial_root;
	bool matches;

	if (!current->nsproxy || !current->fs || !init_task.fs ||
	    !init_nsproxy.mnt_ns ||
	    current->nsproxy->mnt_ns != init_nsproxy.mnt_ns)
		return false;
	get_fs_root(current->fs, &caller_root);
	get_fs_root(init_task.fs, &initial_root);
	matches = path_equal(&caller_root, &initial_root);
	path_put(&initial_root);
	path_put(&caller_root);
	return matches;
}

void susfs_add_sus_path_loop(void __user **user_info)
{
	struct st_susfs_sus_path info = { 0 };
	struct susfs_path_entry *entry;
	int ret;

	ret = susfs_path_request(user_info, &info);
	if (ret)
		goto reply;
	/* Relative names have no stable meaning in a deferred kernel worker. */
	if (info.target_pathname[0] != '/') {
		ret = -EINVAL;
		goto reply;
	}
	if (!susfs_path_loop_context()) {
		ret = -EXDEV;
		goto reply;
	}
	mutex_lock(&susfs_path_lock);
	list_for_each_entry(entry, &susfs_path_entries, node) {
		if (!strcmp(entry->pathname, info.target_pathname))
			goto unlock;
	}
	if (susfs_path_entry_count == SUSFS_PATH_LOOP_MAX_ENTRIES) {
		ret = -E2BIG;
		goto unlock;
	}
	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		ret = -ENOMEM;
		goto unlock;
	}
	memcpy(entry->pathname, info.target_pathname,
	       strlen(info.target_pathname) + 1);
	list_add_tail_rcu(&entry->node, &susfs_path_entries);
	susfs_path_entry_count++;
unlock:
	mutex_unlock(&susfs_path_lock);
	/* Also retry an existing registration when the target finally exists. */
	if (!ret && ksu_susfs_cred_ready())
		schedule_work(&susfs_extra_works);
reply:
	susfs_path_reply(user_info, ret);
}

void susfs_run_sus_path_loop(void)
{
	const struct cred *saved;
	struct list_head *node;
	struct susfs_path_entry *entry;
	int index;

	if (!ksu_susfs_cred_ready() || !ksu_cred || !susfs_path_loop_context())
		return;
	saved = override_creds(ksu_cred);
	index = srcu_read_lock(&susfs_path_srcu);
	/* Name the SRCU domain explicitly for pointer and lockdep checks. */
	for (node = srcu_dereference(susfs_path_entries.next, &susfs_path_srcu);
	     node != &susfs_path_entries;
	     node = srcu_dereference(node->next, &susfs_path_srcu)) {
		entry = list_entry(node, struct susfs_path_entry, node);
		/* No writer mutex is held across potentially blocking filesystem IO. */
		susfs_path_mark(entry->pathname);
	}
	srcu_read_unlock(&susfs_path_srcu, index);
	revert_creds(saved);
}

bool susfs_is_inode_sus_path(struct inode *inode)
{
	if (!inode || !inode->i_mapping || !susfs_is_current_proc_umounted_app())
		return false;
	return test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags) &&
	       !uid_eq(current_uid(), inode->i_uid);
}
EXPORT_SYMBOL_GPL(susfs_is_inode_sus_path);

int susfs_get_data_path(struct path *path)
{
	return kern_path("/data", LOOKUP_FOLLOW, path);
}
