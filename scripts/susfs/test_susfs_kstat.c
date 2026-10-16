/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define __user
#define __aligned(n) __attribute__((aligned(n)))
#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif
#define GFP_KERNEL 0
#define NSEC_PER_SEC 1000000000UL
#define AS_FLAGS_SUS_KSTAT 35
#define MINORBITS 20
#define MKDEV(major,minor) (((major)<<MINORBITS)|(minor))
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define container_of(p,type,member) ((type *)((char *)(p)-offsetof(type,member)))
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int64_t s64;
typedef uint64_t u64;
struct hlist_node { struct hlist_node *next, **pprev; };
struct rcu_head { int unused; };
static int rcu_depth, writer_depth, allocations;
static void *deferred[1000]; static int deferred_count;
static bool allocation_fault, copy_fault, compat_mode;
static unsigned int model_uid;
static bool susfs_is_current_app_uid(void) { return model_uid % 100000 >= 10000; }
static bool in_compat_syscall(void) { return compat_mode; }
#define DEFINE_MUTEX(n) int n
#define mutex_lock(p) do { assert(!*(p)); *(p)=1; writer_depth++; } while(0)
#define mutex_unlock(p) do { assert(*(p)); *(p)=0; writer_depth--; } while(0)
#define DEFINE_HASHTABLE(n,bits) struct hlist_node *n[1U<<(bits)]
#define first_entry(p,type,member) ((p)?container_of((p),type,member):NULL)
#define hash_for_each(t,bkt,obj,member) \
 for((bkt)=0,(obj)=NULL;!(obj)&&(bkt)<ARRAY_SIZE(t);(bkt)++) \
 for((obj)=first_entry((t)[bkt],__typeof__(*(obj)),member);(obj);(obj)=first_entry((obj)->member.next,__typeof__(*(obj)),member))
#define hash_for_each_possible_rcu(t,obj,member,key) \
 for((obj)=first_entry((t)[(key)%ARRAY_SIZE(t)],__typeof__(*(obj)),member);(obj);(obj)=first_entry((obj)->member.next,__typeof__(*(obj)),member))
#define hash_for_each_possible_safe(t,obj,tmp,member,key) \
 for((obj)=first_entry((t)[(key)%ARRAY_SIZE(t)],__typeof__(*(obj)),member); \
 (obj)&&(((tmp)=(obj)->member.next),1);(obj)=first_entry((tmp),__typeof__(*(obj)),member))
static void model_hash_add(struct hlist_node **head,struct hlist_node *node)
{ assert(writer_depth); node->next=*head; node->pprev=head; if(*head)(*head)->pprev=&node->next; *head=node; }
#define hash_add_rcu(t,node,key) model_hash_add(&(t)[(key)%ARRAY_SIZE(t)],node)
static void hash_del_rcu(struct hlist_node *node)
{ assert(writer_depth && node->pprev); *node->pprev=node->next; if(node->next)node->next->pprev=node->pprev; }
static void *kzalloc(size_t n,int flags) { if(allocation_fault)return NULL; void *p=calloc(1,n); assert(p); allocations++; return p; }
static void kfree(void *p) { if(p) { allocations--; free(p); } }
static void flush_rcu(void) { if(!rcu_depth)while(deferred_count)kfree(deferred[--deferred_count]); }
static void rcu_read_lock(void) { rcu_depth++; }
static void rcu_read_unlock(void) { assert(rcu_depth); rcu_depth--; flush_rcu(); }
#define kfree_rcu(p,member) do { assert(deferred_count<1000); deferred[deferred_count++]=(p); flush_rcu(); } while(0)
static size_t copy_from_user(void *dst,const void *src,size_t n)
{ if(copy_fault || !src)return n; memcpy(dst,src,n); return 0; }
static size_t copy_to_user(void *dst,const void *src,size_t n)
{ if(!dst)return n; memcpy(dst,src,n); return 0; }
static bool test_bit(int n,const unsigned long *p) { return !!(*p&(1UL<<n)); }
static void set_bit(int n,unsigned long *p) { *p|=1UL<<n; }
struct address_space { unsigned long flags; };
struct timespec_model { long tv_sec,tv_nsec; };
struct kstat { u32 result_mask,blksize,nlink; u64 ino,blocks; dev_t dev; long long size; struct timespec_model atime,mtime,ctime; };
struct kstatfs { unsigned long f_type,f_bsize,f_blocks,f_bfree,f_frsize,f_flags; };
struct path;
struct dentry;
struct inode_operations { int (*getattr)(const struct path *,struct kstat *,u32,unsigned int); };
struct super_operations { int (*statfs)(struct dentry *,struct kstatfs *); };
struct super_block { dev_t s_dev; unsigned long s_flags,s_magic; struct super_operations *s_op; };
struct inode { unsigned long i_ino; struct address_space *i_mapping; struct super_block *i_sb; struct inode_operations *i_op; };
struct dentry { struct inode *inode; struct super_block *d_sb; };
struct vfsmount { int mnt_flags; struct super_block *mnt_sb; };
struct mount { struct vfsmount mnt; int mnt_id; };
struct path { struct dentry *dentry; struct vfsmount *mnt; };
static struct inode *d_backing_inode(struct dentry *d) { return d->inode; }
static struct mount *real_mount(struct vfsmount *m) { return container_of(m,struct mount,mnt); }
static struct path resolved;
static struct path visible_path;
static bool mount_filter,mount_hidden,visible_exists=true;
static int path_refs,lookup_error,lsm_getattr_error,fs_getattr_error,lsm_statfs_error,fs_statfs_error,visible_lsm_error;
/* Presentation/root reference behavior is modeled here. Native mount helper
 * allocation, parent traversal and ownership have a separate source-body suite.
 */
static bool susfs_mnt_hide_current(void) { return mount_filter; }
static bool susfs_mnt_hidden(struct vfsmount *m) { return m==resolved.mnt && mount_hidden; }
static int susfs_mnt_id(struct vfsmount *m) { return real_mount(m)->mnt_id; }
static int susfs_mnt_nonhidden_id(struct vfsmount *m) { return susfs_mnt_hidden(m) ? 101 : susfs_mnt_id(m); }
static int susfs_mnt_visible_root(struct vfsmount *m,struct path *p) {
    assert(m==resolved.mnt); if (!visible_exists) return -ENOENT; *p=visible_path; ++path_refs; return 0;
}
static int kern_path(const char *name,int flags,struct path *path)
{ assert(name[0] && strlen(name)<256 && flags==0); if(lookup_error)return lookup_error; *path=resolved; path_refs++; return 0; }
static void path_put(struct path *p) { assert(path_refs>0); path_refs--; }
static int security_inode_getattr(const struct path *p) { return lsm_getattr_error; }
static int security_sb_statfs(struct dentry *d) { return d==visible_path.dentry ? visible_lsm_error : lsm_statfs_error; }
static void generic_fillattr(struct inode *inode,struct kstat *s)
{ *s=(struct kstat){.ino=inode->i_ino,.dev=inode->i_sb->s_dev,.nlink=1,.size=100,.blocks=8,.blksize=4096,.atime={10,11},.mtime={12,13},.ctime={14,15}}; }
static int fs_getattr(const struct path *path,struct kstat *s,u32 request,unsigned int query)
{ generic_fillattr(d_backing_inode(path->dentry),s); return fs_getattr_error; }
static int fs_statfs(struct dentry *d,struct kstatfs *s)
{ *s=(struct kstatfs){.f_type=123,.f_bsize=4096,.f_blocks=d->d_sb->s_dev==43 ? 2000 : 1000,.f_bfree=900}; return fs_statfs_error; }
#define STATX_BASIC_STATS 0x7ff
#define STATX_ALL 0xfff
#define STATX_MNT_ID 0x1000
#define KSTAT_QUERY_FLAGS 0x6000
#define MNT_READONLY 1
#define MNT_NOSUID 2
#define MNT_NODEV 4
#define MNT_NOEXEC 8
#define MNT_NOATIME 16
#define MNT_NODIRATIME 32
#define MNT_RELATIME 64
#define SB_SYNCHRONOUS 1
#define SB_MANDLOCK 2
#define SB_RDONLY 4
#define ST_RDONLY 1
#define ST_NOSUID 2
#define ST_NODEV 4
#define ST_NOEXEC 8
#define ST_NOATIME 16
#define ST_NODIRATIME 32
#define ST_RELATIME 64
#define ST_SYNCHRONOUS 128
#define ST_MANDLOCK 256
#define ST_VALID 512

#include "linux/susfs.h"
#include "linux/susfs_meta.h"
#include "native_dev.h"
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
#include "kstat_handler.h"
#endif
#include "native_getattr.h"
#include "native_statfs.h"

#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
static void dispatch(struct st_susfs_sus_kstat *info,bool update)
{
	if(compat_mode) {
		struct susfs_kstat_compat c={0};
#define C(field) c.field=info->field
		C(is_statically); C(target_ino); C(spoofed_ino); C(spoofed_dev); C(spoofed_nlink); C(spoofed_size);
		C(spoofed_atime_tv_sec); C(spoofed_atime_tv_nsec); C(spoofed_mtime_tv_sec); C(spoofed_mtime_tv_nsec);
		C(spoofed_ctime_tv_sec); C(spoofed_ctime_tv_nsec); C(spoofed_blocks); C(spoofed_blksize); C(flags);
#undef C
		memcpy(c.target_pathname,info->target_pathname,sizeof(c.target_pathname));
		void *p=&c; if(update)susfs_update_sus_kstat(&p); else susfs_add_sus_kstat(&p); info->err=c.err;
	} else { void *p=info; if(update)susfs_update_sus_kstat(&p); else susfs_add_sus_kstat(&p); }
}
static struct st_susfs_sus_kstat request(void)
{
	return (struct st_susfs_sus_kstat){.target_pathname="/target",.target_ino=9999,.spoofed_ino=777,.spoofed_dev=0x800,
	.spoofed_nlink=3,.spoofed_size=44,.spoofed_atime_tv_sec=-21,.spoofed_atime_tv_nsec=22,.spoofed_mtime_tv_sec=23,
	.spoofed_mtime_tv_nsec=24,.spoofed_ctime_tv_sec=25,.spoofed_ctime_tv_nsec=26,.spoofed_blocks=27,.spoofed_blksize=512,.flags=0xfff,.err=99};
}
static void check_all(struct kstat *s)
{ assert(s->ino==777 && s->dev==MKDEV(8,0) && s->nlink==3 && s->size==44 && s->atime.tv_sec==-21 && s->atime.tv_nsec==22 && s->mtime.tv_sec==23 && s->mtime.tv_nsec==24 && s->ctime.tv_sec==25 && s->ctime.tv_nsec==26 && s->blocks==27 && s->blksize==512); }
static void test_rule(struct inode *inode)
{
	struct st_susfs_sus_kstat info=request(); struct kstat s; struct kstatfs fs;
	model_uid=0; dispatch(&info,false); assert(!info.err && !path_refs && allocations==1);
	assert(!vfs_getattr(&resolved,&s,STATX_ALL,0) && s.ino==inode->i_ino);
	model_uid=10000; assert(!vfs_getattr(&resolved,&s,STATX_ALL,0)); check_all(&s);
	int mnt=0; unsigned long ino=0; dev_t dev=0;
	assert(susfs_spoof_inode_ids(inode,&dev,&ino,&mnt) && ino==777 && dev==MKDEV(8,0) && mnt==55);
	assert(!vfs_statfs(&resolved,&fs) && fs.f_blocks==1000 && fs.f_frsize==4096);
	/* Every spoof flag changes just its specified field. */
	for(int bit=0;bit<12;bit++) {
		model_uid=0; info=request(); info.flags=1<<bit; dispatch(&info,false); assert(!info.err);
		model_uid=10000; assert(!vfs_getattr(&resolved,&s,STATX_ALL,0));
		assert(s.ino==((bit==0)?777:inode->i_ino)); assert(s.dev==((bit==1)?MKDEV(8,0):inode->i_sb->s_dev));
		assert(s.nlink==((bit==2)?3:1)); assert(s.size==((bit==3)?44:100));
		assert(s.atime.tv_sec==((bit==4)?-21:10)); assert(s.atime.tv_nsec==((bit==5)?22:11));
		assert(s.mtime.tv_sec==((bit==6)?23:12)); assert(s.mtime.tv_nsec==((bit==7)?24:13));
		assert(s.ctime.tv_sec==((bit==8)?25:14)); assert(s.ctime.tv_nsec==((bit==9)?26:15));
		assert(s.blocks==((bit==10)?27:8)); assert(s.blksize==((bit==11)?512:4096));
	}
	model_uid=0; info=request(); dispatch(&info,false); model_uid=10000;
	lsm_getattr_error=-EACCES; assert(vfs_getattr(&resolved,&s,STATX_ALL,0)==-EACCES); lsm_getattr_error=0;
	fs_getattr_error=-EIO; assert(vfs_getattr(&resolved,&s,STATX_ALL,0)==-EIO && s.ino==inode->i_ino); fs_getattr_error=0;
	lsm_statfs_error=-EACCES; assert(vfs_statfs(&resolved,&fs)==-EACCES); lsm_statfs_error=0;
	fs_statfs_error=-EIO; assert(vfs_statfs(&resolved,&fs)==-EIO); fs_statfs_error=0;
	struct inode other=*inode; struct address_space fresh={0}; other.i_mapping=&fresh;
	generic_fillattr(&other,&s); susfs_spoof_kstat(&other,&s,NULL); assert(s.ino==other.i_ino);
	other.i_mapping=inode->i_mapping; struct super_block sb=*inode->i_sb; sb.s_dev++; other.i_sb=&sb;
	generic_fillattr(&other,&s); susfs_spoof_kstat(&other,&s,NULL); assert(s.ino==other.i_ino);
	model_uid=1000; assert(!vfs_getattr(&resolved,&s,STATX_ALL,0) && s.ino==inode->i_ino);
	u64 stat_mount_id=0;
	model_uid=110000; assert(!vfs_getattr_mnt_id(&resolved,&s,STATX_ALL,0,&stat_mount_id)); check_all(&s);
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	assert(stat_mount_id==55 && !(s.result_mask&STATX_MNT_ID));
#endif
	/* Model a reader spanning another context's replacement; old data survives. */
	rcu_read_lock(); struct susfs_kstat_rule *old=susfs_find_kstat(inode); assert(old);
	model_uid=0; info.spoofed_ino=888; dispatch(&info,false); assert(!info.err && deferred_count);
	assert(old->info.spoofed_ino==777); rcu_read_unlock(); assert(allocations==1);
	/* Update after bind/overlay replacement: preserve the original snapshot. */
	unsigned long old_ino=inode->i_ino; inode->i_ino++; real_mount(resolved.mnt)->mnt_id=66;
	info=request(); info.flags=KSTAT_SPOOF_INO; dispatch(&info,true); assert(!info.err);
	model_uid=10000; mnt=0; ino=0; assert(susfs_spoof_inode_ids(inode,NULL,&ino,&mnt) && ino==888 && mnt==55);
	assert(!vfs_getattr_mnt_id(&resolved,&s,STATX_ALL,0,&stat_mount_id) && s.ino==888 && s.size==100);
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	assert(stat_mount_id==55); /* Stored original ID wins over the replaced mount. */
#endif
	model_uid=0; info.flags=0xfff; dispatch(&info,true); assert(!info.err);
	model_uid=10000; assert(!vfs_getattr(&resolved,&s,STATX_ALL,0) && s.size==44);
	inode->i_ino=old_ino; real_mount(resolved.mnt)->mnt_id=55;
}
static void faults(void)
{
	struct st_susfs_sus_kstat info=request(); model_uid=0;
	strcpy(info.target_pathname,"/missing"); dispatch(&info,true); assert(info.err==-ENOENT);
	info=request(); memset(info.target_pathname,'x',sizeof(info.target_pathname)); dispatch(&info,false); assert(info.err==-EINVAL);
	info=request(); info.flags=0x1000; dispatch(&info,false); assert(info.err==-EINVAL);
	info=request(); info.spoofed_atime_tv_nsec=NSEC_PER_SEC; dispatch(&info,false); assert(info.err==-EINVAL);
	info=request(); info.spoofed_size=-1; dispatch(&info,false); assert(info.err==-EINVAL);
	if (!compat_mode) { info=request(); info.spoofed_blksize=0x100000000L; dispatch(&info,false); assert(info.err==-EINVAL); }
	info=request(); copy_fault=true; dispatch(&info,false); assert(info.err==-EFAULT); copy_fault=false;
	allocation_fault=true; dispatch(&info,false); assert(info.err==-ENOMEM); allocation_fault=false;
	lookup_error=-EACCES; dispatch(&info,false); assert(info.err==-EACCES && !path_refs); lookup_error=0;
	if(!compat_mode) { unsigned char *raw=(unsigned char *)&info; raw[offsetof(struct st_susfs_sus_kstat,is_statically)]=2; dispatch(&info,false); assert(info.err==-EINVAL); }
	void *p=NULL; susfs_add_sus_kstat(&p); susfs_add_sus_kstat(NULL); p=(void *)(uintptr_t)-8; susfs_reply_kstat(&p,-EFAULT);
}
static void clear_rules(void)
{
	for(size_t i=0;i<ARRAY_SIZE(susfs_kstat_rules);i++) { struct hlist_node *n=susfs_kstat_rules[i]; while(n) { struct hlist_node *next=n->next; kfree(container_of(n,struct susfs_kstat_rule,node)); n=next; } susfs_kstat_rules[i]=NULL; }
	flush_rcu(); assert(!allocations && !rcu_depth && !writer_depth && !path_refs);
}
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
static void test_mount_consumers(void) {
    struct super_block vsb={.s_dev=43,.s_op=resolved.dentry->d_sb->s_op};
    struct inode vi={.i_sb=&vsb}; struct dentry vd={.inode=&vi,.d_sb=&vsb};
    struct mount vm={.mnt={.mnt_flags=MNT_NOSUID,.mnt_sb=&vsb},.mnt_id=101};
    visible_path=(struct path){.dentry=&vd,.mnt=&vm.mnt};
    struct kstat s; struct kstatfs fs; u64 stat_mount_id=0;
    mount_hidden=true; mount_filter=true;
    assert(!vfs_getattr_mnt_id(&resolved,&s,STATX_MNT_ID,0,&stat_mount_id) && stat_mount_id==101 && !(s.result_mask&STATX_MNT_ID));
    assert(!vfs_statfs(&resolved,&fs) && fs.f_blocks==2000 && (fs.f_flags&ST_NOSUID) && !path_refs);
    lsm_statfs_error=-EPERM; assert(vfs_statfs(&resolved,&fs)==-EPERM && !path_refs); lsm_statfs_error=0;
    visible_lsm_error=-EACCES; assert(vfs_statfs(&resolved,&fs)==-EACCES && !path_refs); visible_lsm_error=0;
    fs_statfs_error=-EIO; assert(vfs_statfs(&resolved,&fs)==-EIO && !path_refs); fs_statfs_error=0;
    visible_exists=false; assert(!vfs_statfs(&resolved,&fs) && fs.f_blocks==1000 && !path_refs); visible_exists=true;
    mount_filter=false; assert(!vfs_statfs(&resolved,&fs) && fs.f_blocks==1000 && !path_refs);
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
    assert(!susfs_statfs_snapshot(&resolved,&fs) && fs.f_blocks==2000 && !path_refs);
#endif
    mount_hidden=false; visible_path=(struct path){0};
}
#endif
int main(void)
{
	struct inode_operations ops={.getattr=fs_getattr}; struct super_operations sbops={.statfs=fs_statfs};
	struct super_block sb={.s_dev=42,.s_op=&sbops}; struct address_space mapping={0};
	struct inode inode={.i_ino=123,.i_mapping=&mapping,.i_sb=&sb,.i_op=&ops};
	struct dentry dentry={.inode=&inode,.d_sb=&sb}; struct mount mount={.mnt={.mnt_sb=&sb},.mnt_id=55};
	resolved=(struct path){.dentry=&dentry,.mnt=&mount.mnt};
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	for(int compat=0;compat<2;compat++) { compat_mode=compat; test_rule(&inode); faults(); clear_rules(); mapping.flags=0; }
#else
	struct kstat s; struct kstatfs fs; assert(!vfs_getattr(&resolved,&s,STATX_ALL,0) && s.ino==123);
	assert(!vfs_statfs(&resolved,&fs) && fs.f_blocks==1000);
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	test_mount_consumers();
#endif
	puts("native/compat payloads, selective fields, LSM/filesystem errors, inode identity/reuse, snapshots and modeled RCU replacement passed");
}
