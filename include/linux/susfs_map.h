/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUSFS_MAP_H
#define _LINUX_SUSFS_MAP_H

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/susfs_def.h>

/* Caller holds mmap_read_lock, or otherwise owns the VMA/file lifetime. */
static inline bool susfs_is_hidden_vma(const struct vm_area_struct *vma)
{
	struct inode *inode;

	if (!vma || !vma->vm_file || !susfs_is_current_proc_umounted_app())
		return false;
	inode = file_inode(vma->vm_file);
	return inode && inode->i_mapping &&
		test_bit(AS_FLAGS_SUS_MAP, &inode->i_mapping->flags);
}
#endif
