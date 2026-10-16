/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUSFS_SPOOF_H
#define _LINUX_SUSFS_SPOOF_H

#include <linux/types.h>

struct new_utsname;
struct seq_file;

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
void susfs_spoof_uname(struct new_utsname *name);
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
bool susfs_spoof_cmdline(struct seq_file *m);
#endif

#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
#include <linux/string.h>

/* Filter only the proc iterator; internal symbol lookup remains intact. */
static inline bool susfs_hide_symbol(const char *name)
{
	static const char * const prefixes[] = {
		"ksu_", "__ksu_", "susfs_", "ksud", "is_ksu_", "is_manager_",
		"escape_to_", "setup_selinux", "track_throne", "on_post_fs_data",
		"try_umount", "kernelsu", "__initcall__kmod_kernelsu",
		"apply_kernelsu", "handle_sepolicy", "getenforce", "setenforce",
		"is_zygote",
	};
	size_t i;

	if (!strncmp(name, "__cfi_", 6))
		name += 6;
	else if (!strncmp(name, "__pfx_", 6))
		name += 6;
	for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
		if (!strncmp(name, prefixes[i], strlen(prefixes[i])))
			return true;
	return false;
}
#endif
#endif
