// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/susfs.h>
#include <linux/susfs_spoof.h>
#include <linux/uaccess.h>

static DEFINE_MUTEX(susfs_cmdline_lock);
static char susfs_cmdline[SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE];
static bool susfs_cmdline_set;

void susfs_set_cmdline_or_bootconfig(void __user **user_info)
{
	struct st_susfs_spoof_cmdline_or_bootconfig *info;
	unsigned long address;
	int err = -EFAULT;
	size_t len;

	if (!user_info || !*user_info)
		return;
	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto reply;
	}
	if (copy_from_user(info, *user_info, sizeof(*info)))
		goto free_info;
	err = -EINVAL;
	len = strnlen(info->fake_cmdline_or_bootconfig,
		      sizeof(info->fake_cmdline_or_bootconfig));
	if (!len || len == sizeof(info->fake_cmdline_or_bootconfig))
		goto free_info;
	mutex_lock(&susfs_cmdline_lock);
	memcpy(susfs_cmdline, info->fake_cmdline_or_bootconfig, len + 1);
	susfs_cmdline_set = true;
	mutex_unlock(&susfs_cmdline_lock);
	err = 0;
free_info:
	kfree(info);
reply:
	if (!check_add_overflow((unsigned long)*user_info,
		(unsigned long)offsetof(struct st_susfs_spoof_cmdline_or_bootconfig, err),
		&address))
		(void)copy_to_user((void __user *)address, &err, sizeof(err));
}

bool susfs_spoof_cmdline(struct seq_file *m)
{
	bool enabled;
	size_t len;

	/* seq output may allocate: use a sleeping lock, never a seqlock writer. */
	mutex_lock(&susfs_cmdline_lock);
	enabled = susfs_cmdline_set;
	if (enabled) {
		len = strlen(susfs_cmdline);
		seq_puts(m, susfs_cmdline);
		if (susfs_cmdline[len - 1] != '\n')
			seq_putc(m, '\n');
	}
	mutex_unlock(&susfs_cmdline_lock);
	return enabled;
}
