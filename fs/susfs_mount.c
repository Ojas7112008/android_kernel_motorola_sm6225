// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/jump_label.h>
#include <linux/overflow.h>
#include <linux/susfs.h>
#include <linux/susfs_mount.h>
#include <linux/uaccess.h>

extern bool susfs_is_current_ksu_domain(void);
static DEFINE_STATIC_KEY_FALSE(susfs_hide_mounts_non_su);

bool susfs_mnt_hide_current(void)
{
	return susfs_is_current_proc_umounted();
}

bool susfs_mnt_hide_proc(void)
{
	return susfs_mnt_hide_current() ||
		(static_branch_unlikely(&susfs_hide_mounts_non_su) &&
		 !susfs_is_current_ksu_domain());
}

void susfs_set_hide_sus_mnts_for_non_su_procs(void __user **arg)
{
	unsigned char request[sizeof(struct st_susfs_hide_sus_mnts_for_non_su_procs)];
	unsigned long address;
	int err = -EFAULT;

	if (!arg || !*arg)
		return;
	if (copy_from_user(request, *arg, sizeof(request)))
		goto reply;
	err = -EINVAL;
	if (request[offsetof(struct st_susfs_hide_sus_mnts_for_non_su_procs, enabled)] > 1)
		goto reply;
	if (request[offsetof(struct st_susfs_hide_sus_mnts_for_non_su_procs, enabled)])
		static_branch_enable(&susfs_hide_mounts_non_su);
	else
		static_branch_disable(&susfs_hide_mounts_non_su);
	err = 0;
reply:
	if (!check_add_overflow((unsigned long)*arg,
		(unsigned long)offsetof(struct st_susfs_hide_sus_mnts_for_non_su_procs, err),
		&address))
		(void)copy_to_user((void __user *)address, &err, sizeof(err));
}
