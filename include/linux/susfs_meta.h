/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUSFS_META_H
#define _LINUX_SUSFS_META_H

#include <linux/types.h>
struct inode;
struct kstat;
struct kstatfs;
struct path;

void susfs_spoof_kstat(struct inode *inode, struct kstat *stat, u64 *mnt_id);
bool susfs_spoof_statfs(struct inode *inode, struct kstatfs *stat);
bool susfs_spoof_inode_ids(struct inode *inode, dev_t *dev,
			   unsigned long *ino, int *mnt_id);
int susfs_statfs_snapshot(const struct path *path, struct kstatfs *stat);
#endif
