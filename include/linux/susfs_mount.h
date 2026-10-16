/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUSFS_MOUNT_H
#define _LINUX_SUSFS_MOUNT_H
#include <linux/types.h>

struct vfsmount;
struct path;
bool susfs_mnt_hide_current(void);
bool susfs_mnt_hide_proc(void);
bool susfs_mnt_hidden(struct vfsmount *mnt);
int susfs_mnt_id(struct vfsmount *mnt);
int susfs_mnt_parent_id(struct vfsmount *mnt);
int susfs_mnt_nonhidden_id(struct vfsmount *mnt);
/* Success returns one mount and one dentry reference; release with path_put. */
int susfs_mnt_visible_root(struct vfsmount *mnt, struct path *path);
#endif
