/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUSFS_FUSE_H
#define _LINUX_SUSFS_FUSE_H

#include <linux/list.h>
#include <linux/types.h>
#include <linux/uidgid.h>

struct inode;
struct super_block;

/* Embedded in the private FUSE inode; no additional inode or mount reference. */
struct susfs_fuse_inode {
	struct hlist_node node;
	struct inode *inode;
	u64 ino;
	kuid_t uid;
};

void susfs_fuse_inode_init(struct susfs_fuse_inode *entry, struct inode *inode);
void susfs_fuse_inode_update(struct susfs_fuse_inode *entry, u64 ino);
void susfs_fuse_inode_forget(struct susfs_fuse_inode *entry);
bool susfs_fuse_is_hidden(struct super_block *sb, u64 ino);

#endif
