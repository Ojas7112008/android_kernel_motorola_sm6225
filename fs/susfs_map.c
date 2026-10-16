// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/overflow.h>
#include <linux/string.h>
#include <linux/susfs.h>
#include <linux/uaccess.h>

void susfs_add_sus_map(void __user **user_info)
{
	struct st_susfs_sus_map info;
	struct path path;
	struct inode *inode;
	unsigned long address;
	int err = -EFAULT;

	if (!user_info || !*user_info)
		return;
	if (copy_from_user(&info, *user_info, sizeof(info)))
		goto reply;
	err = -EINVAL;
	if (!info.target_pathname[0] ||
	    strnlen(info.target_pathname, sizeof(info.target_pathname)) ==
						sizeof(info.target_pathname))
		goto reply;
	err = kern_path(info.target_pathname, LOOKUP_FOLLOW, &path);
	if (err)
		goto reply;
	inode = d_backing_inode(path.dentry);
	err = -ENOENT;
	if (inode && inode->i_mapping) {
		set_bit(AS_FLAGS_SUS_MAP, &inode->i_mapping->flags);
		err = 0;
	}
	path_put(&path);
reply:
	if (!check_add_overflow((unsigned long)*user_info,
		(unsigned long)offsetof(struct st_susfs_sus_map, err), &address))
		(void)copy_to_user((void __user *)address, &err, sizeof(err));
}
