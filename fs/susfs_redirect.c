// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/overflow.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/susfs.h>
#include <linux/susfs_redirect.h>
#include <linux/uaccess.h>

extern bool susfs_is_current_ksu_domain(void);

/* One immutable record publishes forward and reverse identity together. */
struct susfs_redirect_rule {
	struct list_head list;
	struct rcu_head rcu;
	unsigned long from_ino, to_ino;
	dev_t from_dev, to_dev;
	struct st_susfs_open_redirect info;
};
static LIST_HEAD(susfs_redirect_rules);
static DEFINE_MUTEX(susfs_redirect_lock);

static bool susfs_redirect_scheme(int scheme)
{
	switch (scheme) {
	case UID_NON_APP_PROC:
		return current_uid().val % 100000 < 10000;
	case UID_ROOT_PROC_EXCEPT_SU_PROC:
		return current_uid().val == 0 && !susfs_is_current_ksu_domain();
	case UID_NON_SU_PROC:
		return !susfs_is_current_ksu_domain();
	case UID_UMOUNTED_APP_PROC:
		return susfs_is_current_proc_umounted_app();
	case UID_UMOUNTED_PROC:
		return susfs_is_current_proc_umounted();
	default:
		return false;
	}
}

bool susfs_redirect_may_apply(void)
{
	struct susfs_redirect_rule *rule;
	bool found = false;

	rcu_read_lock();
	list_for_each_entry_rcu(rule, &susfs_redirect_rules, list)
		if (susfs_redirect_scheme(rule->info.uid_scheme)) {
			found = true;
			break;
		}
	rcu_read_unlock();
	return found;
}

static bool susfs_redirect_marked(struct inode *inode)
{
	return inode && inode->i_mapping &&
		test_bit(AS_FLAGS_OPEN_REDIRECT, &inode->i_mapping->flags);
}

void susfs_add_open_redirect(void __user **arg)
{
	struct st_susfs_open_redirect info;
	struct susfs_redirect_rule *rule, *entry, *old = NULL;
	struct path from, to;
	struct inode *from_inode, *to_inode;
	int err = -EFAULT;
	unsigned long address;

	if (!arg || !*arg)
		return;
	if (copy_from_user(&info, *arg, sizeof(info)))
		goto reply;
	err = -EINVAL;
	if (info.target_pathname[0] != '/' || info.redirected_pathname[0] != '/' ||
	    strnlen(info.target_pathname, sizeof(info.target_pathname)) ==
						sizeof(info.target_pathname) ||
	    strnlen(info.redirected_pathname, sizeof(info.redirected_pathname)) ==
						sizeof(info.redirected_pathname) ||
	    info.uid_scheme < UID_NON_APP_PROC || info.uid_scheme > UID_UMOUNTED_PROC)
		goto reply;
	rule = kzalloc(sizeof(*rule), GFP_KERNEL);
	if (!rule) {
		err = -ENOMEM;
		goto reply;
	}
	err = kern_path(info.target_pathname, 0, &from);
	if (err)
		goto free_rule;
	err = kern_path(info.redirected_pathname, 0, &to);
	if (err)
		goto put_from;
	from_inode = d_backing_inode(from.dentry);
	to_inode = d_backing_inode(to.dentry);
	err = -ENOENT;
	if (!from_inode || !to_inode || !from_inode->i_mapping || !to_inode->i_mapping)
		goto put_to;
	err = -EOPNOTSUPP;
	if (from_inode->i_sb->s_magic == FUSE_SUPER_MAGIC ||
	    to_inode->i_sb->s_magic == FUSE_SUPER_MAGIC)
		goto put_to;
	rule->from_ino = from_inode->i_ino;
	rule->from_dev = from_inode->i_sb->s_dev;
	rule->to_ino = to_inode->i_ino;
	rule->to_dev = to_inode->i_sb->s_dev;
	rule->info = info;
	err = -EINVAL;
	if (rule->from_ino == rule->to_ino && rule->from_dev == rule->to_dev)
		goto put_to;

	mutex_lock(&susfs_redirect_lock);
	list_for_each_entry(entry, &susfs_redirect_rules, list) {
		if (!strcmp(entry->info.target_pathname, info.target_pathname)) {
			old = entry;
			continue;
		}
		/* Reject chains/cycles and ambiguous reverse aliases. */
		if ((entry->from_dev == rule->from_dev && entry->from_ino == rule->from_ino) ||
		    (entry->to_dev == rule->from_dev && entry->to_ino == rule->from_ino) ||
		    (entry->from_dev == rule->to_dev && entry->from_ino == rule->to_ino) ||
		    (entry->to_dev == rule->to_dev && entry->to_ino == rule->to_ino)) {
			err = -EEXIST;
			goto unlock;
		}
	}
	set_bit(AS_FLAGS_OPEN_REDIRECT, &from_inode->i_mapping->flags);
	set_bit(AS_FLAGS_OPEN_REDIRECT, &to_inode->i_mapping->flags);
	if (old) {
		list_replace_rcu(&old->list, &rule->list);
		kfree_rcu(old, rcu);
	} else {
		list_add_rcu(&rule->list, &susfs_redirect_rules);
	}
	mutex_unlock(&susfs_redirect_lock);
	path_put(&to);
	path_put(&from);
	err = 0;
	goto reply;
unlock:
	mutex_unlock(&susfs_redirect_lock);
put_to:
	path_put(&to);
put_from:
	path_put(&from);
free_rule:
	kfree(rule);
reply:
	if (!check_add_overflow((unsigned long)*arg,
			       (unsigned long)offsetof(struct st_susfs_open_redirect, err),
			       &address))
		(void)copy_to_user((void __user *)address, &err, sizeof(err));
}

struct filename *susfs_redirect_filename(struct inode *inode)
{
	struct susfs_redirect_rule *rule;
	char name[SUSFS_MAX_LEN_PATHNAME];
	bool found = false;

	if (!susfs_redirect_marked(inode))
		return NULL;
	rcu_read_lock();
	list_for_each_entry_rcu(rule, &susfs_redirect_rules, list) {
		if (rule->from_ino == inode->i_ino && rule->from_dev == inode->i_sb->s_dev &&
		    susfs_redirect_scheme(rule->info.uid_scheme)) {
			memcpy(name, rule->info.redirected_pathname, sizeof(name));
			found = true;
			break;
		}
	}
	rcu_read_unlock();
	/* getname_kernel may sleep: never allocate inside an RCU read section. */
	return found ? getname_kernel(name) : NULL;
}

bool susfs_redirect_view(struct inode *inode, struct susfs_redirect_view *view)
{
	struct susfs_redirect_rule *rule;
	bool found = false;

	if (!susfs_is_current_proc_umounted_app() || !susfs_redirect_marked(inode))
		return false;
	rcu_read_lock();
	list_for_each_entry_rcu(rule, &susfs_redirect_rules, list) {
		if (rule->to_ino == inode->i_ino && rule->to_dev == inode->i_sb->s_dev) {
			view->ino = rule->from_ino;
			view->dev = rule->from_dev;
			memcpy(view->name, rule->info.target_pathname, sizeof(view->name));
			found = true;
			break;
		}
	}
	rcu_read_unlock();
	return found;
}

int susfs_redirect_readlink(struct inode *inode, char __user *buffer, int buflen)
{
	struct susfs_redirect_view view;
	size_t len;

	if (!susfs_redirect_view(inode, &view))
		return -ENOENT;
	if (buflen <= 0)
		return -EINVAL;
	len = min_t(size_t, strlen(view.name), buflen);
	/* readlink returns copied bytes, does not append NUL, and permits truncation. */
	return copy_to_user(buffer, view.name, len) ? -EFAULT : len;
}
