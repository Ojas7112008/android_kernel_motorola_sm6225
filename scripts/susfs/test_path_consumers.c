// SPDX-License-Identifier: GPL-2.0
/* Host-only models surround actual source functions; no native VFS execution. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#define __user
#define likely(x) (x)
#define unlikely(x) (x)
#define BIT(x) (1UL << (x))
#define EXPORT_SYMBOL(x)
#define ALIGN(x, a) (((x)+(a)-1)&~((a)-1))
#define container_of(p, t, m) ((t *)((char *)(p)-offsetof(t,m)))
#define ERR_PTR(x) ((void *)(intptr_t)(x))
#define PTR_ERR(p) ((long)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define IS_ERR_OR_NULL(p) (!(p) || IS_ERR(p))
#define BUG_ON(x) assert(!(x))
static bool model_warn(bool condition) { return condition; }
#define WARN_ON(x) model_warn(!!(x))
#define VERIFY_WRITE 1
#define __ARCH_WANT_OLD_READDIR 1
#define FUSE_SUPER_MAGIC 0x65735546
#define OVERLAYFS_SUPER_MAGIC 0x794c7630
#define DCACHE_OP_HASH 1
#define DCACHE_OP_WEAK_REVALIDATE 2
#define LOOKUP_RCU 1
#define LOOKUP_ROOT 2
#define LOOKUP_JUMPED 4
#define LOOKUP_NO_REVAL 8
#define LOOKUP_DIRECTORY 16
#define LOOKUP_FOLLOW 32
#define MAY_EXEC 1
#define MAY_NOT_BLOCK 2
#define MAY_READ 4
#define FMODE_CREATED 1
#define FMODE_OPENED 2
#define AUDIT_TYPE_CHILD_CREATE 1
#define WALK_FOLLOW 1
#define WALK_MORE 2
#define LAST_NORM 0
#define LAST_DOT 1
#define LAST_DOTDOT 2
#ifndef O_PATH
#define O_PATH 010000000
#endif

typedef uint64_t u64;
typedef uint32_t u32;
typedef unsigned short umode_t;
typedef uint32_t compat_ulong_t;
typedef int32_t compat_long_t;
struct list_head { struct list_head *next, *prev; };
typedef struct { unsigned val; } kuid_t;
static kuid_t uid = { 10001 };
static unsigned long thread_flags = BIT(33);
static kuid_t current_uid(void) { return uid; }
static bool uid_eq(kuid_t a, kuid_t b) { return a.val == b.val; }
static bool test_thread_flag(int n) { return !!(thread_flags & BIT(n)); }
static void set_thread_flag(int n) { thread_flags |= BIT(n); }
static void clear_thread_flag(int n) { thread_flags &= ~BIT(n); }
static bool test_bit(int n, const unsigned long *p) { return !!(__atomic_load_n(p,__ATOMIC_RELAXED) & BIT(n)); }
struct task_struct { int unused; };
static void clear_tsk_thread_flag(struct task_struct *t,int n) { (void)t;(void)n; }
struct address_space { unsigned long flags; };
struct qstr { union { struct { u32 hash, len; }; u64 hash_len; }; const char *name; };
struct inode; struct dentry; struct file; struct dir_context;
struct inode_operations {
 struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned);
 int (*create)(struct inode *, struct dentry *, umode_t, bool);
 int (*atomic_open)(struct inode *, struct dentry *, struct file *, unsigned, umode_t);
};
struct super_block { unsigned long s_magic, s_flags; unsigned s_blocksize_bits; };
struct inode {
 struct super_block *i_sb; struct address_space *i_mapping; kuid_t i_uid, i_gid;
 const struct inode_operations *i_op; umode_t i_mode; unsigned long i_ino;
 int refs, i_rwsem; bool dead;
 unsigned long i_state, i_flags, i_blocks, i_nlink, i_rdev; loff_t i_size; unsigned i_blkbits;
 struct { long tv_sec, tv_nsec; } i_atime, i_mtime, i_ctime;
 struct address_space i_data;
};
struct dentry_operations { int (*d_hash)(const struct dentry *, struct qstr *); int (*d_weak_revalidate)(struct dentry *, unsigned); };
struct dentry {
 struct inode *d_inode; struct super_block *d_sb; struct dentry *d_parent;
 const struct dentry_operations *d_op; struct qstr d_name;
 unsigned d_flags, d_seq; int refs; bool in_lookup, cached, invalidated;
};
struct vfsmount { int refs; };
struct path { struct dentry *dentry; struct vfsmount *mnt; };
struct dir_context { int (*actor)(struct dir_context *, const char *, int, loff_t, u64, unsigned); loff_t pos; };
struct file_operations { int (*iterate_shared)(struct file *,struct dir_context *); int (*iterate)(struct file *,struct dir_context *); };
struct file { struct path f_path; struct inode *f_inode; const struct file_operations *f_op; loff_t f_pos; unsigned f_mode; void *private_data; };
struct fd { struct file *file; };
struct open_flags { int open_flag, acc_mode, intent; umode_t mode; };
struct saved { const char *name; };
struct nameidata { struct path path, root; struct inode *inode; struct qstr last; unsigned flags, seq, depth; int last_type; struct saved stack[4]; };
struct linux_dirent64 { u64 d_ino; int64_t d_off; unsigned short d_reclen; unsigned char d_type; char d_name[]; };
#define SYSCALL_DEFINE3(n,t1,a1,t2,a2,t3,a3) long sys_##n(t1 a1,t2 a2,t3 a3)
#define COMPAT_SYSCALL_DEFINE3(n,t1,a1,t2,a2,t3,a3) long compat_sys_##n(t1 a1,t2 a2,t3 a3)
#define IS_DEADDIR(i) ((i)->dead)
#define IS_POSIXACL(i) false
#define DECLARE_WAIT_QUEUE_HEAD_ONSTACK(w) int w = 0
static struct super_block sb = { .s_magic = 0xef53 };
static struct address_space hidden_mapping = { .flags = BIT(33) }, normal_mapping;
static struct inode hidden = { .i_mapping=&hidden_mapping, .i_uid={0}, .i_mode=S_IFREG, .i_ino=9, .refs=1 };
static struct inode visible = { .i_mapping=&normal_mapping, .i_uid={0}, .i_mode=S_IFREG, .i_ino=10, .refs=1 };
static struct inode directory;
static struct dentry parent, cached_child, alias, pool[256];
static unsigned used;
static struct vfsmount mount = { .refs=1 }, child_mount = { .refs=1 };
static int reval=1, weak_reval=1, alloc_error, lookup_error, hash_error, permission_error, security_error;
static bool lookup_alias, seq_retry, parent_retry, unlazy_error, mount_cross, signals, cache_miss;
static int dputs, lookup_done_calls, lookup_calls, creates, atomic_calls, permissions, fd_refs, access_calls, copies;
static int create_errno, notifications, open_checks, open_errno, ilookups;
static bool create_marks_inode, last_excl;
static struct inode *resolved_inode, *created_inode;
#include "native_backing_inode.h"
static struct dentry *dget(struct dentry *d) { assert(d->refs>0); d->refs++; return d; }
static void dput(struct dentry *d) { assert(d && !IS_ERR(d) && d->refs>0); d->refs--; dputs++; }
static void mntput(struct vfsmount *m) { assert(m->refs>0); m->refs--; }
static void path_put(struct path *p) { dput(p->dentry); mntput(p->mnt); }
static bool d_is_negative(struct dentry *d) { return !d->d_inode; }
static bool d_is_symlink(struct dentry *d) { return d->d_inode && S_ISLNK(d->d_inode->i_mode); }
static bool d_can_lookup(struct dentry *d) { return d->d_inode && S_ISDIR(d->d_inode->i_mode); }
static bool d_in_lookup(struct dentry *d) { return d->in_lookup; }
static void d_lookup_done(struct dentry *d) { if(d->in_lookup) { assert(d->refs>0); d->in_lookup=false; lookup_done_calls++; } }
static void d_invalidate(struct dentry *d) { d->invalidated=true; d->cached=false; }
static int d_revalidate(struct dentry *d, unsigned flags) { (void)d; (void)flags; int r=reval; if(r==0) reval=1; return r; }
static struct dentry *d_lookup(const struct dentry *d, const struct qstr *q) {
 assert(d==&parent);
 if(cache_miss || !cached_child.cached || q->len!=cached_child.d_name.len || memcmp(q->name,cached_child.d_name.name,q->len)) return NULL;
 return dget(&cached_child);
}
static struct dentry *__d_lookup(const struct dentry *d, const struct qstr *q) { return d_lookup(d,q); }
static struct dentry *__d_lookup_rcu(const struct dentry *d, const struct qstr *q,unsigned *seq) { struct dentry *p=d_lookup(d,q); if(p) { p->refs--; *seq=p->d_seq; } return p; }
static unsigned full_name_hash(const struct dentry *d,const char *s,unsigned len) { (void)d; unsigned h=0; for(unsigned i=0;i<len;i++) h=h*31+(unsigned char)s[i]; return h; }
#include "dcache_hash.h"
static struct inode *ilookup(struct super_block *s, unsigned long ino) { ilookups++;if(s==hidden.i_sb && ino==hidden.i_ino) { hidden.refs++; return &hidden; } return NULL; }
static void iput(struct inode *i) { if(!i)return;assert(i->refs>1); i->refs--; }
static struct dentry *d_alloc(struct dentry *d,const struct qstr *q) { if(alloc_error) return NULL; assert(used<256); struct dentry *p=&pool[used++]; *p=(struct dentry){.refs=1,.d_parent=d,.d_sb=d->d_sb,.d_name=*q}; return p; }
static struct dentry *d_alloc_parallel(struct dentry *d,const struct qstr *q,void *w) { (void)w; if(alloc_error) return ERR_PTR(-ENOMEM); struct dentry *p=d_lookup(d,q); if(p) return p; p=d_alloc(d,q); p->in_lookup=true; return p; }
static struct dentry *model_lookup(struct inode *i,struct dentry *d,unsigned flags) { (void)i; (void)flags; lookup_calls++; if(lookup_error) return ERR_PTR(lookup_error); if(lookup_alias) return dget(&alias); d->d_inode=resolved_inode; return NULL; }
static int model_create(struct inode *i,struct dentry *d,umode_t mode,bool excl)
{
 (void)i; (void)mode; creates++; last_excl=excl;
 if(create_errno) return create_errno;
 d->d_inode=created_inode;
 if(create_marks_inode) {
  d->d_inode->i_mapping=&d->d_inode->i_data;
  __atomic_store_n(&d->d_inode->i_mapping->flags,BIT(33),__ATOMIC_RELAXED);
 }
 return 0;
}
/* A modeled filesystem's finish_no_open / finish_open ownership contract. */
static int model_atomic(struct inode *i,struct dentry *d,struct file *f,unsigned flags,umode_t mode)
{
 struct dentry *res=NULL, *target=d;
 int err;
 atomic_calls++;
 if(d_in_lookup(d)) {
  res=model_lookup(i,d,0);
  if(IS_ERR(res)) return PTR_ERR(res);
  if(res) target=res;
 }
 if(!(flags&O_CREAT) || target->d_inode) {
  f->f_path.dentry=res;
  return 0;
 }
 err=model_create(i,target,mode,flags&O_EXCL);
 if(!err) {
  f->f_path.dentry=dget(target);
  f->f_mode|=FMODE_CREATED|FMODE_OPENED;
 }
 if(res) dput(res);
 return err;
}
static const struct inode_operations iops = {.lookup=model_lookup,.create=model_create,.atomic_open=model_atomic};
static int inode_permission2(struct vfsmount *m,struct inode *i,int flags) { (void)i;(void)flags; assert(m==&mount); permissions++; return permission_error; }
static int unlazy_walk(struct nameidata *nd) { if(unlazy_error) return -ECHILD; nd->flags &=~LOOKUP_RCU; return 0; }
static int unlazy_child(struct nameidata *nd,struct dentry *d,unsigned seq) { (void)seq; if(unlazy_error) return -ECHILD; nd->flags &=~LOOKUP_RCU; dget(d); return 0; }
static bool read_seqcount_retry(unsigned *p,unsigned seq) { (void)p;(void)seq; return seq_retry; }
static bool __read_seqcount_retry(unsigned *p,unsigned seq) { (void)p;(void)seq; return parent_retry; }
static bool __follow_mount_rcu(struct nameidata *nd,struct path *p,struct inode **i,unsigned *seq) { (void)nd;(void)seq; if(mount_cross) { p->dentry=&alias;p->mnt=&child_mount;*i=alias.d_inode; } return true; }
static int follow_managed(struct path *p,struct nameidata *nd) { (void)nd; if(mount_cross) { dput(p->dentry); p->dentry=dget(&alias);p->mnt=&child_mount;child_mount.refs++; } return 1; }
static void inode_lock_shared(struct inode *i) { i->i_rwsem++; }
static void inode_unlock_shared(struct inode *i) { assert(i->i_rwsem>0);i->i_rwsem--; }
static void inode_unlock(struct inode *i) { inode_unlock_shared(i); }
static void success_walk_trace(struct nameidata *nd) { (void)nd; }
static int weak_validate(struct dentry *d,unsigned flags) { (void)d;(void)flags;return weak_reval; }
static int model_hash(const struct dentry *d,struct qstr *q) { (void)d;(void)q;return hash_error; }
static const struct dentry_operations dops = {.d_hash=model_hash,.d_weak_revalidate=weak_validate};
static void put_link(struct nameidata *nd) { assert(nd->depth);nd->depth--; }
static int pick_link(struct nameidata *nd,struct path *p,struct inode *i,unsigned seq) { (void)i;(void)seq; nd->path=*p;return 1; }
static int handle_dots(struct nameidata *nd,int type) { (void)nd;(void)type; return 0; }
static const char *get_link(struct nameidata *nd) { (void)nd;return "target"; }
static u64 hash_name(struct dentry *d,const char *s) { unsigned n=0;while(s[n] && s[n]!='/')n++;return ((u64)n<<32)|full_name_hash(d,s,n); }
#define hashlen_len(v) ((u32)((v)>>32))
static unsigned current_umask(void) { return 022; }
static int may_o_create(const struct path *p,struct dentry *d,umode_t mode) { (void)p;(void)d;(void)mode;return permission_error; }
static int may_open(const struct path *p,int access,int flags) { (void)p;(void)access;(void)flags;open_checks++;return open_errno; }
static void audit_inode_child(struct inode *i,struct dentry *d,int type) { (void)i;(void)d;(void)type; }
static void fsnotify_create(struct inode *i,struct dentry *d) { (void)i;(void)d;notifications++; }
static struct inode *file_inode(struct file *f) { return f->f_inode; }
static int security_file_permission(struct file *f,int mask) { (void)f;(void)mask;return security_error; }
static int down_read_killable(int *r) { (*r)++;return 0; }
static int down_write_killable(int *r) { (*r)++;return 0; }
static void fsnotify_access(struct file *f) { (void)f; }
static void file_accessed(struct file *f) { (void)f; }
static bool signal_pending(void *c) { (void)c;return signals; }
#define current NULL
static int fault_after=-1;
static bool fault(void) { if(fault_after<0) return false;return !fault_after--; }
static int model_put(void *p,const void *v,size_t n) { copies++; if(fault())return -EFAULT;memcpy(p,v,n);return 0; }
#define __put_user(v,p) ({ typeof(*(p)) val=(v);model_put((p),&val,sizeof(val)); })
#define put_user(v,p) __put_user(v,p)
static unsigned long copy_to_user(void *p,const void *v,size_t n) { return !!model_put(p,v,n); }
#define __copy_to_user copy_to_user
static bool access_ok(int type,void *p,unsigned long n) { (void)type;(void)p;(void)n;access_calls++;return true; }
static struct file *open_file;
static struct fd fdget_pos(unsigned fd) { if(fd==3 && open_file) { fd_refs++;return (struct fd){open_file}; }return (struct fd){0}; }
static void fdput_pos(struct fd f) { assert(f.file && fd_refs>0);fd_refs--; }
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
#include "fuse_index_model.h"
#include <linux/susfs.h>
#include "fuse_index_under_test.h"
#include "inode_predicate.h"
#endif
#include "namei_under_test.h"
#include "readdir_under_test.h"

static bool enabled(void) {
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
 return true;
#else
 return false;
#endif
}
static void reset(void) {
 uid.val=10001;thread_flags=BIT(33);reval=weak_reval=1;
 alloc_error=lookup_error=hash_error=permission_error=security_error=0;
 lookup_alias=seq_retry=parent_retry=unlazy_error=mount_cross=signals=cache_miss=false;
 dputs=lookup_done_calls=lookup_calls=creates=atomic_calls=permissions=fd_refs=access_calls=copies=used=0;fault_after=-1;
 create_errno=notifications=open_checks=open_errno=ilookups=0;create_marks_inode=last_excl=false;
 hidden_mapping.flags=BIT(33);normal_mapping.flags=0;visible.i_data.flags=0;
 hidden.i_mapping=&hidden_mapping;visible.i_mapping=&normal_mapping;created_inode=&visible;
 hidden.i_sb=visible.i_sb=&sb;hidden.i_uid.val=0;hidden.refs=visible.refs=1;hidden.i_mode=S_IFREG;
 directory=(struct inode){.i_sb=&sb,.i_mapping=&normal_mapping,.i_mode=S_IFDIR,.i_op=&iops,.refs=1};
 parent=(struct dentry){.d_inode=&directory,.d_sb=&sb,.refs=1,.d_op=&dops};
 cached_child=(struct dentry){.d_inode=&hidden,.d_sb=&sb,.refs=1,.cached=true,.d_parent=&parent,.d_name={.name="secret",.len=6}};
 alias=(struct dentry){.d_inode=&hidden,.d_sb=&sb,.refs=1,.d_parent=&parent,.d_name={.name="secret",.len=6}};
 resolved_inode=&hidden;mount.refs=child_mount.refs=1;sb.s_magic=0xef53;open_file=NULL;
}
static struct nameidata nd_new(unsigned flags) { return (struct nameidata){.path={&parent,&mount},.inode=&directory,.flags=flags,.last={.name="secret",.len=6}}; }
static void assert_refs(void) { assert(cached_child.refs==1 && alias.refs==1 && hidden.refs==1 && directory.i_rwsem==0 && fd_refs==0); for(unsigned i=0;i<used;i++)assert(pool[i].refs==0 && !pool[i].in_lookup); }
static void release_result(struct dentry *d) { if(!IS_ERR_OR_NULL(d))dput(d); }
static void test_lookups(void) {
 struct path p={0};struct inode *i;unsigned seq=0;struct nameidata nd;
 reset();nd=nd_new(0);int r=lookup_fast(&nd,&p,&i,&seq);assert(r==(enabled()?-ENOENT:1));if(r==1)dput(p.dentry);assert_refs();
 reset();nd=nd_new(LOOKUP_RCU);r=lookup_fast(&nd,&p,&i,&seq);assert(r==(enabled()?-ECHILD:1));assert(dputs==0 && lookup_done_calls==0);assert_refs();
 for(unsigned n=0;n<2;n++) { reset();nd=nd_new(LOOKUP_RCU);seq_retry=!n;parent_retry=n;r=lookup_fast(&nd,&p,&i,&seq);assert(r==-ECHILD && dputs==0);assert_refs(); }
 reset();cached_child.d_inode=&visible;nd=nd_new(0);assert(lookup_fast(&nd,&p,&i,&seq)==1 && i==&visible);dput(p.dentry);assert_refs();
 reset();cached_child.d_inode=NULL;nd=nd_new(0);assert(lookup_fast(&nd,&p,&i,&seq)==-ENOENT);assert_refs();
 reset();cached_child.d_inode=&visible;nd=nd_new(0);reval=-ESTALE;assert(lookup_fast(&nd,&p,&i,&seq)==-ESTALE);assert_refs();
 reset();cache_miss=true;nd=nd_new(LOOKUP_RCU);unlazy_error=true;assert(lookup_fast(&nd,&p,&i,&seq)==-ECHILD);assert_refs();
 for(unsigned uid_case=0;uid_case<4;uid_case++) { reset();uid.val=uid_case==0?0:uid_case==1?1000:uid_case==2?10001:110001;if(uid_case==2)thread_flags=0;if(uid_case==3)hidden.i_uid=uid;nd=nd_new(0);assert(lookup_fast(&nd,&p,&i,&seq)==1);dput(p.dentry);assert_refs(); }
 for(unsigned cold=0;cold<3;cold++) {
  reset();cache_miss=cold!=0;lookup_alias=cold==2;nd=nd_new(0);struct dentry *d=__lookup_hash(&nd.last,&parent,0);assert(enabled()?PTR_ERR(d)==-ENOENT:!IS_ERR(d));release_result(d);assert_refs();
  reset();cache_miss=cold!=0;lookup_alias=cold==2;nd=nd_new(0);d=lookup_slow(&nd.last,&parent,0);assert(enabled()?PTR_ERR(d)==-ENOENT:!IS_ERR(d));release_result(d);assert_refs();
 }
 reset();cache_miss=true;lookup_error=-EIO;nd=nd_new(0);assert(PTR_ERR(lookup_slow(&nd.last,&parent,0))==-EIO);assert(lookup_done_calls==1);assert_refs();
 reset();alloc_error=1;cache_miss=true;nd=nd_new(0);assert(PTR_ERR(__lookup_hash(&nd.last,&parent,0))==-ENOMEM);assert_refs();
 reset();reval=0;resolved_inode=&visible;nd=nd_new(0);struct dentry *d=lookup_slow(&nd.last,&parent,0);assert(!IS_ERR(d) && d->d_inode==&visible && cached_child.invalidated);dput(d);assert_refs();
 reset();reval=-EIO;nd=nd_new(0);assert(PTR_ERR(lookup_dcache(&nd.last,&parent,0))==-EIO);assert_refs();
 dputs=0;
}
static void test_walks(void) {
 struct path p={0};struct inode *i;unsigned seq;int r;
 reset();cached_child.d_inode=&visible;mount_cross=true;struct nameidata nd=nd_new(0);
 r=lookup_fast(&nd,&p,&i,&seq);assert(r==1 && i==&hidden && p.mnt==&child_mount);
 r=step_into(&nd,&p,0,i,seq);assert(r==(enabled()?-ENOENT:0));assert(nd.path.dentry==&alias && parent.refs==0);path_put(&nd.path);assert(child_mount.refs==1);assert_refs();
 reset();cached_child.d_inode=&visible;mount_cross=true;nd=nd_new(LOOKUP_RCU);assert(lookup_fast(&nd,&p,&i,&seq)==1);r=step_into(&nd,&p,0,i,seq);assert(r==(enabled()?-ECHILD:0));assert(dputs==0);assert_refs();
 reset();parent.d_inode=&hidden;nd=nd_new(0);nd.inode=&hidden;
 permission_error=-EACCES;assert(link_path_walk("child",&nd)==-EACCES);assert(permissions==1);
 permission_error=0;r=link_path_walk("child",&nd);assert(r==(enabled()?-ENOENT:0));
 reset();parent.d_inode=&hidden;nd=nd_new(0);assert(complete_walk(&nd)==(enabled()?-ENOENT:0));
 reset();parent.d_inode=&hidden;parent.d_flags=DCACHE_OP_WEAK_REVALIDATE;nd=nd_new(LOOKUP_JUMPED);weak_reval=-EIO;assert(complete_walk(&nd)==-EIO);
 reset();parent.d_inode=&hidden;parent.d_flags=DCACHE_OP_WEAK_REVALIDATE;nd=nd_new(LOOKUP_JUMPED);weak_reval=0;assert(complete_walk(&nd)==-ESTALE);
 reset();parent.d_inode=&hidden;parent.d_flags=DCACHE_OP_WEAK_REVALIDATE;nd=nd_new(LOOKUP_JUMPED);assert(complete_walk(&nd)==(enabled()?-ENOENT:0));
 reset();cached_child.d_inode=&visible;nd=nd_new(0);p=(struct path){dget(&cached_child),&mount};visible.i_mode=S_IFLNK;assert(step_into(&nd,&p,0,&visible,0)==0);dput(nd.path.dentry);assert_refs();
 reset();cached_child.d_inode=&visible;nd=nd_new(0);p=(struct path){dget(&cached_child),&mount};visible.i_mode=S_IFLNK;assert(step_into(&nd,&p,WALK_FOLLOW,&visible,0)==1);dput(p.dentry);assert_refs();
}
static void test_open(void) {
 for(unsigned cold=0;cold<3;cold++)for(unsigned variant=0;variant<3;variant++) {
  reset();cache_miss=cold!=0;lookup_alias=cold==2;struct nameidata nd=nd_new(0);struct path p={0};struct file f={0};
  struct open_flags op={.open_flag=variant==0?O_RDONLY:variant==1?O_CREAT|O_EXCL:O_CREAT|O_TRUNC,.mode=0600};
  int r=lookup_open(&nd,&p,&f,&op,true);assert(r==(enabled()?-ENOENT:0));if(!r)dput((f.f_mode&FMODE_OPENED)?f.f_path.dentry:p.dentry);
  if(enabled())assert(creates==0 && atomic_calls==0 && !(f.f_mode&(FMODE_CREATED|FMODE_OPENED)));assert_refs();
 }
 reset();cache_miss=true;resolved_inode=NULL;struct nameidata nd=nd_new(0);struct path p;struct file f={0};struct open_flags op={.open_flag=O_CREAT,.mode=0600};
 int r=lookup_open(&nd,&p,&f,&op,true);assert(r==0);assert(enabled()?creates==1:atomic_calls==1);dput((f.f_mode&FMODE_OPENED)?f.f_path.dentry:p.dentry);assert_refs();
 reset();cache_miss=true;resolved_inode=&visible;nd=nd_new(0);f=(struct file){0};r=lookup_open(&nd,&p,&f,&op,true);assert(r==0);dput(p.dentry);assert_refs();
 reset();cache_miss=true;resolved_inode=NULL;nd=nd_new(0);f=(struct file){0};r=lookup_open(&nd,&p,&f,&op,false);assert(r==-EROFS);assert_refs();
 reset();uid.val=0;cache_miss=true;nd=nd_new(0);f=(struct file){0};r=lookup_open(&nd,&p,&f,&op,true);assert(r==0 && atomic_calls==1);dput(p.dentry);assert_refs();
 reset();cache_miss=true;lookup_error=-EIO;nd=nd_new(0);f=(struct file){0};r=lookup_open(&nd,&p,&f,&op,false);assert(r==-EIO);assert_refs();
}
static void test_post_create(void)
{
 for(unsigned cached=0;cached<3;cached++)for(unsigned flags=0;flags<3;flags++) {
  struct open_flags op={.open_flag=O_CREAT|(flags==1?O_EXCL:flags==2?O_TRUNC:0),.mode=0600};
  struct path p={0};struct file f={0};int result;
  reset();cache_miss=cached!=0;cached_child.d_inode=NULL;resolved_inode=NULL;
  lookup_alias=cached==2;alias.d_inode=NULL;created_inode=&hidden;
  struct nameidata nd=nd_new(0);
  result=lookup_open(&nd,&p,&f,&op,true);
  assert(result==(enabled()?-ENOENT:0));
  assert(creates==1 && notifications==1 && last_excl==(flags==1));
  if(enabled()) assert(!p.dentry && !(f.f_mode&FMODE_OPENED) && atomic_calls==0 && open_checks==0);
  else dput((f.f_mode&FMODE_OPENED)?f.f_path.dentry:p.dentry);
  assert_refs();
 }
 /* A marker published during create is re-read before path transfer. */
 reset();cache_miss=true;resolved_inode=NULL;create_marks_inode=true;
 struct nameidata nd=nd_new(0);struct path p={0};struct file f={0};
 struct open_flags op={.open_flag=O_CREAT|O_TRUNC,.mode=0600};
 int result=lookup_open(&nd,&p,&f,&op,true);
 assert(result==(enabled()?-ENOENT:0));
 if(!result)dput((f.f_mode&FMODE_OPENED)?f.f_path.dentry:p.dentry);
 assert_refs();
 /* The marked inode's owner, root and an unclassified task retain access. */
 for(unsigned who=0;who<3;who++) {
  reset();cache_miss=true;resolved_inode=NULL;created_inode=&hidden;
  if(who==0)hidden.i_uid=uid;else if(who==1)uid.val=0;else thread_flags=0;
  nd=nd_new(0);p=(struct path){0};f=(struct file){0};
  result=lookup_open(&nd,&p,&f,&op,true);assert(result==0 && notifications==1);
  dput((f.f_mode&FMODE_OPENED)?f.f_path.dentry:p.dentry);assert_refs();
 }
 /* Native provider errors win, and unsuccessful creation emits no event. */
 for(unsigned error=0;error<3;error++) {
  reset();cache_miss=true;resolved_inode=NULL;created_inode=&hidden;
  create_errno=error==0?-EIO:error==1?-EEXIST:-ENOSPC;
  nd=nd_new(0);p=(struct path){0};f=(struct file){0};
  assert(lookup_open(&nd,&p,&f,&op,true)==create_errno);
  assert(notifications==0 && !(f.f_mode&FMODE_OPENED));assert_refs();
 }
 /* Native atomic-open permission errors retain an opened-file reference
  * until caller fput cleanup; the fixture balances that reference explicitly.
  */
 reset();uid.val=0;cache_miss=true;resolved_inode=NULL;open_errno=-EACCES;
 nd=nd_new(0);p=(struct path){0};f=(struct file){0};
 assert(lookup_open(&nd,&p,&f,&op,true)==-EACCES);
 assert((f.f_mode&FMODE_OPENED) && open_checks==1 && notifications==1);
 dput(f.f_path.dentry);assert_refs();
}
struct entry { const char *name;u64 ino; };
static struct entry entries[4];static int entry_count;
static int model_iterate(struct file *f,struct dir_context *ctx) { (void)f;while(ctx->pos<entry_count) { int n=ctx->pos; if(ctx->actor(ctx,entries[n].name,strlen(entries[n].name),n+1,entries[n].ino,8))break;ctx->pos++; }return 0; }
static const struct file_operations fops={.iterate_shared=model_iterate};
static struct file dirfile;
static void setup_entries(void) { dirfile=(struct file){.f_path={&parent,&mount},.f_inode=&directory,.f_op=&fops};open_file=&dirfile;entries[0]=(struct entry){"secret",9};entries[1]=(struct entry){"public",10};entry_count=2; }
static void test_directory_boundaries(void) {
 _Alignas(16) unsigned char buffer[512];long r;
 /* Skips before, between and after visible records leave cookies untouched. */
 reset();setup_entries();entries[0]=(struct entry){"public",10};entries[1]=(struct entry){"secret",9};entries[2]=(struct entry){"other",10};entry_count=3;
 r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && dirfile.f_pos==3);
 struct linux_dirent64 *first=(void *)buffer;
 if(enabled()) { struct linux_dirent64 *last=(void *)(buffer+first->d_reclen);assert(first->d_off==3 && last->d_off==3 && !strcmp(last->d_name,"other"));assert(r==first->d_reclen+last->d_reclen); }
 assert_refs();
 /* A full buffer followed only by hidden entries reaches EOF successfully. */
 reset();setup_entries();entries[0]=(struct entry){"public",10};entries[1]=(struct entry){"secret",9};
 int size=ALIGN(offsetof(struct linux_dirent64,d_name)+strlen("public")+1,sizeof(u64));
 r=ksys_getdents64(3,(void *)buffer,size);assert(r==size && dirfile.f_pos==(enabled()?2:1));
 r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(enabled()?r==0:r>0);assert(dirfile.f_pos==2);assert_refs();
 /* Exercise each native record/final-cookie copy fault, including partial lists. */
 for(int f=0;f<15;f++) {
  reset();setup_entries();entries[0]=(struct entry){"public",10};entries[1]=(struct entry){"other",10};fault_after=f;
  r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r==-EFAULT || r>0);assert_refs();
 }
#ifdef CONFIG_COMPAT
 /* Hidden cached entries bypass a compat inode-width error. */
 reset();setup_entries();entries[0].ino=1ULL<<40;entry_count=1;
 r=compat_sys_old_readdir(3,(void *)buffer,sizeof(buffer));assert(r==(enabled()?0:-EOVERFLOW));assert_refs();
 reset();setup_entries();entries[0].ino=1ULL<<40;entry_count=1;
 r=compat_sys_getdents(3,(void *)buffer,sizeof(buffer));assert(r==(enabled()?0:-EOVERFLOW));assert_refs();
#endif
 /* EOF has no writes, even when the next userspace copy would fault. */
 reset();setup_entries();entry_count=1;fault_after=0;memset(buffer,0xaa,sizeof(buffer));
 r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r==(enabled()?0:-EFAULT));if(enabled())for(unsigned n=0;n<sizeof(buffer);n++)assert(buffer[n]==0xaa);assert_refs();
}

static void test_directories(void) {
 _Alignas(16) unsigned char buffer[512];long r;
 reset();setup_entries();memset(buffer,0xcc,sizeof(buffer));r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));
 struct linux_dirent64 *d=(void *)buffer;assert(r>0 && !strcmp(d->d_name,enabled()?"public":"secret"));
 if(enabled())assert(d->d_off==2 && r==d->d_reclen);assert(dirfile.f_pos==2);assert_refs();
 reset();setup_entries();entry_count=1;memset(buffer,0xcc,sizeof(buffer));r=ksys_getdents64(3,(void *)buffer,1);assert(r==(enabled()?0:-EINVAL));assert(dirfile.f_pos==(enabled()?1:0));assert(copies==0);assert_refs();
 reset();setup_entries();entry_count=1;r=sys_getdents(3,(void *)buffer,sizeof(buffer));assert(enabled()?r==0:r>0);assert_refs();
 reset();setup_entries();r=sys_old_readdir(3,(void *)buffer,sizeof(buffer));assert(r==1);struct old_linux_dirent *o=(void *)buffer;assert(!strcmp(o->d_name,enabled()?"public":"secret"));assert_refs();
#ifdef CONFIG_COMPAT
 reset();setup_entries();r=compat_sys_getdents(3,(void *)buffer,sizeof(buffer));assert(r>0);struct compat_linux_dirent *c=(void *)buffer;assert(!strcmp(c->d_name,enabled()?"public":"secret"));assert_refs();
 reset();setup_entries();r=compat_sys_old_readdir(3,(void *)buffer,sizeof(buffer));assert(r==1);struct compat_old_linux_dirent *co=(void *)buffer;assert(!strcmp(co->d_name,enabled()?"public":"secret"));assert_refs();
 reset();setup_entries();entries[0]=(struct entry){"public",1ULL<<40};entry_count=1;assert(compat_sys_getdents(3,(void *)buffer,sizeof(buffer))==-EOVERFLOW);assert_refs();
#endif
 reset();setup_entries();cache_miss=true;sb.s_magic=FUSE_SUPER_MAGIC;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,"secret"));assert_refs();
 reset();setup_entries();cache_miss=true;sb.s_magic=OVERLAYFS_SUPER_MAGIC;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,"secret") && ilookups==0);assert_refs();
 reset();setup_entries();sb.s_magic=OVERLAYFS_SUPER_MAGIC;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,enabled()?"public":"secret"));assert_refs();
 reset();setup_entries();sb.s_magic=FUSE_SUPER_MAGIC;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,enabled()?"public":"secret"));assert_refs();
 reset();setup_entries();cache_miss=true;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,enabled()?"public":"secret"));assert_refs();
 reset();setup_entries();parent.d_flags=DCACHE_OP_HASH;hash_error=-EINVAL;sb.s_magic=FUSE_SUPER_MAGIC;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0);assert_refs();
 reset();setup_entries();entries[0]=(struct entry){".",9};entries[1]=(struct entry){"..",9};r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,"."));assert_refs();
 reset();setup_entries();security_error=-EACCES;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r==-EACCES && copies==0 && dirfile.f_pos==0);assert_refs();
 reset();setup_entries();entries[0]=(struct entry){"bad/name",9};assert(ksys_getdents64(3,(void *)buffer,sizeof(buffer))==-EIO);assert_refs();
 reset();setup_entries();fault_after=0;assert(ksys_getdents64(3,(void *)buffer,sizeof(buffer))==-EFAULT);assert_refs();
 reset();setup_entries();uid.val=0;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,"secret"));assert_refs();
 reset();setup_entries();hidden.i_uid=uid;r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,"secret"));assert_refs();
 reset();setup_entries();entries[0]=(struct entry){"public",10};entries[1]=(struct entry){"secret",9};signals=true;
 r=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(r>0);assert(dirfile.f_pos==(enabled()?2:1));assert_refs();
 reset();setup_entries();entry_count=1;copies=0;assert(ksys_getdents64(999,(void *)buffer,sizeof(buffer))==-EBADF && copies==0);assert_refs();
}
int main(void) { test_lookups();test_walks();test_open();test_post_create();test_directories();test_directory_boundaries();puts("Actual namei/cache/atomic-open and directory consumers: references, RCU retry, post-create filtering/notifications, native/compat records and offsets passed");return 0; }
