// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/string.h>
#include <linux/susfs.h>
#include <linux/susfs_spoof.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>

static DEFINE_MUTEX(susfs_uname_lock);
static struct st_susfs_uname susfs_uname;
static bool susfs_uname_set;

void susfs_set_uname(void __user **user_info)
{
	struct st_susfs_uname info;
	unsigned long address;
	int err = -EFAULT;

	if (!user_info || !*user_info)
		return;
	if (copy_from_user(&info, *user_info, sizeof(info)))
		goto reply;
	err = -EINVAL;
	if (!info.release[0] || !info.version[0] ||
	    strnlen(info.release, sizeof(info.release)) == sizeof(info.release) ||
	    strnlen(info.version, sizeof(info.version)) == sizeof(info.version))
		goto reply;

	/* Snapshot the caller's UTS namespace before publishing either field. */
	down_read(&uts_sem);
	if (!strcmp(info.release, "default"))
		strscpy(info.release, utsname()->release, sizeof(info.release));
	if (!strcmp(info.version, "default"))
		strscpy(info.version, utsname()->version, sizeof(info.version));
	up_read(&uts_sem);
	mutex_lock(&susfs_uname_lock);
	memcpy(susfs_uname.release, info.release, sizeof(info.release));
	memcpy(susfs_uname.version, info.version, sizeof(info.version));
	susfs_uname_set = true;
	mutex_unlock(&susfs_uname_lock);
	err = 0;
reply:
	if (!check_add_overflow((unsigned long)*user_info,
		(unsigned long)offsetof(struct st_susfs_uname, err), &address))
		(void)copy_to_user((void __user *)address, &err, sizeof(err));
}

void susfs_spoof_uname(struct new_utsname *name)
{
	mutex_lock(&susfs_uname_lock);
	if (susfs_uname_set) {
		memcpy(name->release, susfs_uname.release, sizeof(name->release));
		memcpy(name->version, susfs_uname.version, sizeof(name->version));
	}
	mutex_unlock(&susfs_uname_lock);
}
