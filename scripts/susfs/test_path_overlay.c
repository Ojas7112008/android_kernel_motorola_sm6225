// SPDX-License-Identifier: GPL-2.0
/* Actual overlay/dentry helpers with explicit host object/ref/lock models. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdatomic.h>

#define BIT(n) (1UL << (n))
#define likely(x) (x)
#define unlikely(x) (x)
#define READ_ONCE(x) __atomic_load_n(&(x), __ATOMIC_RELAXED)
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define smp_wmb() atomic_thread_fence(memory_order_release)
#define smp_rmb() atomic_thread_fence(memory_order_acquire)
#include "native_overlay_constants.h"
typedef struct { unsigned val; } kuid_t;
struct address_space { unsigned long flags; };
struct model_time { long tv_sec, tv_nsec; };
struct inode {
 struct address_space *i_mapping, i_data;
 kuid_t i_uid, i_gid; unsigned i_mode, i_flags; unsigned long i_ino;
 struct model_time i_atime, i_mtime, i_ctime;
 int64_t i_size; int refs; bool unhashed; void *i_private;
};
struct dentry;
struct dentry_operations { struct dentry *(*d_real)(struct dentry *,const struct inode *); };
struct dentry { struct inode *d_inode; unsigned d_flags; void *d_fsdata; const struct dentry_operations *d_op; };
/* Private host views supply only fields referenced by the extracted functions. */
struct ovl_inode { unsigned long flags; struct inode vfs_inode; struct dentry *__upperdentry; struct inode *lower, *lowerdata; };
struct ovl_path { struct dentry *dentry; };
struct ovl_entry { unsigned numlower; struct ovl_path lowerstack[2]; };
static kuid_t uid={10001};
static unsigned long thread_flags=BIT(33);
static unsigned warnings, insertions;
static bool model_warn(bool condition) { if(condition)warnings++;return condition; }
#define WARN_ON(x) model_warn(!!(x))
#define WARN(x,...) model_warn(!!(x))
static kuid_t current_uid(void) { return uid; }
static bool uid_eq(kuid_t a,kuid_t b) { return a.val==b.val; }
static bool test_thread_flag(unsigned n) { return !!(thread_flags&BIT(n)); }
static bool test_bit(unsigned n,const unsigned long *p) { return !!(__atomic_load_n(p,__ATOMIC_RELAXED)&BIT(n)); }
static void set_bit(unsigned n,unsigned long *p) { __atomic_fetch_or(p,BIT(n),__ATOMIC_RELAXED); }
static bool d_is_reg(struct dentry *d) { return d->d_inode && S_ISREG(d->d_inode->i_mode); }
static int64_t i_size_read(struct inode *i) { return i->i_size; }
static void i_size_write(struct inode *i,int64_t size) { i->i_size=size; }
static void inode_set_flags(struct inode *i,unsigned flags,unsigned mask) { i->i_flags=(i->i_flags&~mask)|(flags&mask); }
static struct inode *igrab(struct inode *i) { assert(i && i->refs>0);i->refs++;return i; }
static void iput(struct inode *i) { if(i){assert(i->refs>1);i->refs--;} }
static bool inode_unhashed(struct inode *i) { return i->unhashed; }
static void __insert_inode_hash(struct inode *i,unsigned long hash) { (void)hash;assert(i->unhashed);i->unhashed=false;insertions++; }
#include "native_overlay_helpers.h"
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
#include "actual_path_predicate.h"
static bool hidden(struct dentry *d) { return susfs_is_inode_sus_path(d_backing_inode(d)); }
#else
static bool hidden(struct dentry *d) { (void)d;return false; }
#endif

static bool enabled(void)
{
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
 return true;
#else
 return false;
#endif
}
static const struct dentry_operations overlay_ops={.d_real=ovl_d_real};
static void init_inode(struct inode *inode,unsigned mode)
{
 memset(inode,0,sizeof(*inode));inode->i_mapping=&inode->i_data;
 inode->i_mode=mode;inode->refs=1;inode->i_ino=71;inode->i_size=4096;
}
static void init_view(struct ovl_inode *view,struct dentry *upper,struct dentry *lower,struct dentry *data)
{
 memset(view,0,sizeof(*view));init_inode(&view->vfs_inode,S_IFREG);
 view->vfs_inode.i_ino=0;
 ovl_inode_init(&view->vfs_inode,upper,lower,data);
}
static void release_view(struct ovl_inode *view)
{
 iput(view->lowerdata);iput(view->lower);assert(view->vfs_inode.refs==1);
}
static struct dentry view_dentry(struct ovl_inode *view,struct ovl_entry *entry)
{
 return (struct dentry){.d_inode=&view->vfs_inode,.d_flags=DCACHE_OP_REAL,.d_op=&overlay_ops,.d_fsdata=entry};
}

static void test_copy_up_identity(void)
{
 struct inode lower,upper;struct ovl_inode first,second;
 init_inode(&lower,S_IFREG);init_inode(&upper,S_IFREG);
 struct dentry ld={.d_inode=&lower},ud={.d_inode=&upper};
 struct ovl_entry entry={.numlower=1,.lowerstack={{&ld}}};
 init_view(&first,NULL,&ld,NULL);init_view(&second,NULL,&ld,NULL);
 struct dentry one=view_dentry(&first,&entry),two=view_dentry(&second,&entry);
 assert(d_backing_inode(&one)==&first.vfs_inode && d_real_inode(&one)==&lower);
 set_bit(33,&first.vfs_inode.i_mapping->flags);
 assert(hidden(&one)==enabled() && !hidden(&two) && !hidden(&ld));
 /* Copy-up publishes an upper inode without replacing the overlay mapping. */
 first.vfs_inode.unhashed=true;
 ovl_inode_update(&first.vfs_inode,&ud);
 assert(insertions==1 && first.vfs_inode.i_private==&upper);
 assert(first.vfs_inode.i_mapping==&first.vfs_inode.i_data);
 assert(test_bit(33,&first.vfs_inode.i_mapping->flags));
 assert(ovl_inode_real(&first.vfs_inode)==&upper);
 assert(d_real_inode(&one)==&lower); /* metadata-only upper, lower still holds data */
 set_bit(OVL_UPPERDATA,&first.flags);
 assert(d_real_inode(&one)==&upper && ovl_inode_realdata(&first.vfs_inode)==&upper);
 assert(hidden(&one)==enabled() && !hidden(&two) && !hidden(&ud));
 ovl_copyattr(&upper,&first.vfs_inode);ovl_copyflags(&upper,&first.vfs_inode);
 assert(test_bit(33,&first.vfs_inode.i_mapping->flags));
 first.vfs_inode.i_uid=uid;assert(!hidden(&one));first.vfs_inode.i_uid.val=0;
 uid.val=0;assert(!hidden(&one));uid.val=10001;
 thread_flags=0;assert(!hidden(&one));thread_flags=BIT(33);
 /* Marking a physical alias does not mark either overlay view. */
 first.vfs_inode.i_data.flags=0;set_bit(33,&lower.i_data.flags);
 assert(!hidden(&one) && !hidden(&two) && hidden(&ld)==enabled());
 release_view(&first);release_view(&second);assert(lower.refs==1 && upper.refs==1);
}

static void test_metacopy_and_nested_views(void)
{
 struct inode metadata,data;struct ovl_inode meta,inner,outer;
 init_inode(&metadata,S_IFREG);init_inode(&data,S_IFREG);
 struct dentry md={.d_inode=&metadata},dd={.d_inode=&data};
 struct ovl_entry entry={.numlower=2,.lowerstack={{&md},{&dd}}};
 init_view(&meta,NULL,&md,&dd);struct dentry metad=view_dentry(&meta,&entry);
 assert(d_real_inode(&metad)==&data && ovl_inode_realdata(&meta.vfs_inode)==&data);
 set_bit(33,&meta.vfs_inode.i_data.flags);
 assert(hidden(&metad)==enabled() && !hidden(&md) && !hidden(&dd));
 /* A native recursive data lookup and the SUSFS view identity are distinct. */
 struct ovl_entry lower_entry={.numlower=1,.lowerstack={{&dd}}};
 init_view(&inner,NULL,&dd,NULL);struct dentry id=view_dentry(&inner,&lower_entry);
 struct ovl_entry nested_entry={.numlower=1,.lowerstack={{&id}}};
 init_view(&outer,NULL,&id,NULL);struct dentry od=view_dentry(&outer,&nested_entry);
 assert(d_real_inode(&od)==&data);
 set_bit(33,&outer.vfs_inode.i_data.flags);
 assert(hidden(&od)==enabled() && !hidden(&id) && !hidden(&dd));
 release_view(&outer);release_view(&inner);release_view(&meta);
 assert(metadata.refs==1 && data.refs==1);
}

int main(void)
{
 test_copy_up_identity();test_metacopy_and_nested_views();assert(warnings==0);
 puts("Native overlay helpers: view-local markers, copy-up/metacopy/nested identities, owner gates and references passed (host models)");
 return 0;
}
