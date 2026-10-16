// SPDX-License-Identifier: GPL-2.0
/* Live FUSE reported-inode index for SUS_PATH directory consumers. */
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/ksu_susfs.h>
#include <linux/spinlock.h>
#include <linux/susfs_def.h>
#include <linux/susfs_fuse.h>

#if !defined(CONFIG_KSU_SUSFS_SUS_PATH) || !defined(CONFIG_KSU_SUSFS) || \
	!defined(CONFIG_KSU) || !defined(CONFIG_KSU_DEVON_SELINUX_POLICY) || \
	!defined(CONFIG_ARM64) || defined(MODULE)
#error "devon SUSFS FUSE index requires its checked built-in SUS_PATH route"
#endif

static DEFINE_HASHTABLE(susfs_fuse_inodes, 8);
static DEFINE_SPINLOCK(susfs_fuse_lock);

static u64 susfs_fuse_key(struct super_block *sb, u64 ino)
{
	return (u64)(unsigned long)sb ^ ino;
}

void susfs_fuse_inode_init(struct susfs_fuse_inode *entry, struct inode *inode)
{
	INIT_HLIST_NODE(&entry->node);
	entry->inode = inode;
	entry->ino = 0;
	entry->uid = GLOBAL_ROOT_UID;
}
EXPORT_SYMBOL_GPL(susfs_fuse_inode_init);

/* Called under fi->lock after the native attribute/owner update. */
void susfs_fuse_inode_update(struct susfs_fuse_inode *entry, u64 ino)
{
	unsigned long flags;

	spin_lock_irqsave(&susfs_fuse_lock, flags);
	hash_del(&entry->node);
	entry->ino = ino;
	entry->uid = entry->inode->i_uid;
	hash_add(susfs_fuse_inodes, &entry->node,
		 susfs_fuse_key(entry->inode->i_sb, ino));
	spin_unlock_irqrestore(&susfs_fuse_lock, flags);
}
EXPORT_SYMBOL_GPL(susfs_fuse_inode_update);

/* Unlink before eviction changes the mapping and again on direct free. */
void susfs_fuse_inode_forget(struct susfs_fuse_inode *entry)
{
	unsigned long flags;

	spin_lock_irqsave(&susfs_fuse_lock, flags);
	hash_del(&entry->node);
	spin_unlock_irqrestore(&susfs_fuse_lock, flags);
}
EXPORT_SYMBOL_GPL(susfs_fuse_inode_forget);

bool susfs_fuse_is_hidden(struct super_block *sb, u64 ino)
{
	struct susfs_fuse_inode *entry;
	struct inode *inode;
	unsigned long flags;
	bool found = false;

	if (!susfs_is_current_proc_umounted_app())
		return false;
	/* Readers hold no pointer after unlock; teardown uses this same lock. */
	spin_lock_irqsave(&susfs_fuse_lock, flags);
	hash_for_each_possible(susfs_fuse_inodes, entry, node,
			       susfs_fuse_key(sb, ino)) {
		inode = entry->inode;
		if (inode->i_sb != sb || entry->ino != ino)
			continue;
		/* A reused reported number must not hide an unmarked/owned peer. */
		if (!inode->i_mapping || uid_eq(current_uid(), entry->uid) ||
		    !test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags)) {
			found = false;
			break;
		}
		found = true;
	}
	spin_unlock_irqrestore(&susfs_fuse_lock, flags);
	return found;
}
