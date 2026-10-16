/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUSFS_REDIRECT_H
#define _LINUX_SUSFS_REDIRECT_H
#include <linux/types.h>
#include <linux/susfs_def.h>

struct inode;
struct filename;
struct susfs_redirect_view {
	unsigned long ino;
	dev_t dev;
	char name[SUSFS_MAX_LEN_PATHNAME];
};

bool susfs_redirect_may_apply(void);
/* NULL means no match; otherwise returns an owned name or ERR_PTR. */
struct filename *susfs_redirect_filename(struct inode *inode);
/* Copy all metadata under RCU; no borrowed rule pointer escapes. */
bool susfs_redirect_view(struct inode *inode, struct susfs_redirect_view *view);
int susfs_redirect_readlink(struct inode *inode, char __user *buffer, int buflen);
#endif
