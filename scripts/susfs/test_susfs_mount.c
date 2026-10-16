/* Native mount bodies; explicit, single-threaded host ownership/lock models. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#define __user
#define GFP_KERNEL 0
#define GFP_ATOMIC 0
#define BIT(n) (1U << (n))
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define ERR_PTR(e) ((void *)(intptr_t)(e))
#define PTR_ERR(p) ((int)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define READ_ONCE(v) (v)
#define max_t(t,a,b) ((t)(a) > (t)(b) ? (t)(a) : (t)(b))
#define check_add_overflow(a,b,p) __builtin_add_overflow(a,b,p)
#include "payload.h"
struct list_head { struct list_head *next, *prev; };
static void INIT_LIST_HEAD(struct list_head *l) { l->next = l->prev = l; }
static bool list_empty(struct list_head *l) { return l->next == l; }
static void list_add(struct list_head *n, struct list_head *h) {
    n->next = h->next; n->prev = h; h->next->prev = n; h->next = n;
}
static void list_add_tail(struct list_head *n, struct list_head *h) { list_add(n, h->prev); }
static void list_del_init(struct list_head *n) { n->prev->next = n->next; n->next->prev = n->prev; INIT_LIST_HEAD(n); }
struct mount;
struct hlist_node { struct mount *next; };
struct hlist_head { struct mount *first; };
#define INIT_HLIST_NODE(n) ((n)->next = NULL)
#define hlist_for_each_entry_rcu(p,h,m) for ((p)=(h)->first; (p); (p)=(p)->m.next)
struct super_block;
struct dentry { struct super_block *d_sb; int refs; };
struct vfsmount { void *data; unsigned int mnt_flags; struct super_block *mnt_sb; struct dentry *mnt_root; int refs; };
struct fs_context;
struct super_operations { void *(*clone_mnt_data)(void *); void (*update_mnt_data)(void *, struct fs_context *); };
struct super_block { struct super_operations *s_op; int s_active; struct list_head s_mounts; };
struct file_system_type { void *(*alloc_mnt_data)(void); };
struct fs_context { struct dentry *root; const char *source; struct file_system_type *fs_type; unsigned long sb_flags; };
struct fs_pin { int unused; };
struct mnt_pcp { int mnt_count, mnt_writers; };
struct susfs_mnt_id_owner;
struct mount {
    struct vfsmount mnt;
    struct mount *mnt_parent, *mnt_master;
    struct dentry *mnt_mountpoint;
    const char *mnt_devname;
    struct mnt_pcp *mnt_pcp;
    int mnt_count, mnt_writers, mnt_id, mnt_group_id;
    struct hlist_node mnt_hash, mnt_mp_list;
    struct list_head mnt_child, mnt_mounts, mnt_list, mnt_expire, mnt_share, mnt_slave_list,
                     mnt_slave, mnt_umounting, mnt_instance;
    struct fs_pin mnt_umount;
};
#include "mount_storage.h"
static struct mount *real_mount(struct vfsmount *m) { return (void *)((char *)m - offsetof(struct mount, mnt)); }
#define CL_EXPIRE 1
#define CL_SLAVE 2
#define CL_COPY_UNBINDABLE 4
#define CL_MAKE_SHARED 8
#define CL_PRIVATE 16
#define CL_SHARED_TO_SLAVE 32
#define CL_UNPRIVILEGED 64
#define CL_COPY_MNT_NS BIT(25)
#define MNT_READONLY BIT(0)
#define MNT_NODEV BIT(1)
#define MNT_NOSUID BIT(2)
#define MNT_NOEXEC BIT(3)
#define MNT_WRITE_HOLD BIT(4)
#define MNT_MARKED BIT(5)
#define MNT_INTERNAL BIT(6)
#define MNT_LOCK_ATIME BIT(7)
#define MNT_LOCK_READONLY BIT(8)
#define MNT_LOCK_NODEV BIT(9)
#define MNT_LOCK_NOSUID BIT(10)
#define MNT_LOCK_NOEXEC BIT(11)
#define MNT_LOCKED BIT(12)
#define MNT_SHARED BIT(13)
#define SB_KERNMOUNT 1
#define IS_MNT_SHARED(m) ((m)->mnt.mnt_flags & MNT_SHARED)
#define IS_MNT_SLAVE(m) ((m)->mnt_master)
#define CLEAR_MNT_SHARED(m) ((m)->mnt.mnt_flags &= ~MNT_SHARED)
static void set_mnt_shared(struct mount *m) { m->mnt.mnt_flags |= MNT_SHARED; }
#define atomic_inc(p) (++*(p))
static int mem_live, cache_fail, name_fail, pcp_fail, owner_fail, vendor_fail, vendor_updates;
static void *mem_alloc(size_t n) { void *p = calloc(1,n); assert(p); ++mem_live; return p; }
static void mem_free(void *p) { if (p) { assert(mem_live > 0); --mem_live; free(p); } }
static void *kmalloc(size_t n, int flags) { if (owner_fail) { --owner_fail; return NULL; } return mem_alloc(n); }
#define kfree(p) mem_free(p)
#define kfree_const(p) mem_free((void *)(p))
static int mnt_cache;
static void *kmem_cache_zalloc(void *cache, int flags) { if (cache_fail) { --cache_fail; return NULL; } return mem_alloc(TEST_MOUNT_CACHE_SIZE); }
#define kmem_cache_zalloc(c,f) kmem_cache_zalloc(&(c),(f))
static void kmem_cache_free(void *cache, void *p) { mem_free(p); }
#define kmem_cache_free(c,p) kmem_cache_free(&(c),(p))
static const char *kstrdup_const(const char *s, int flags) {
    if (name_fail) { --name_fail; return NULL; } char *p = mem_alloc(strlen(s)+1); strcpy(p,s); return p;
}
static void *pcp_alloc(void) { if (pcp_fail) { --pcp_fail; return NULL; } return mem_alloc(sizeof(struct mnt_pcp)); }
#define alloc_percpu(t) pcp_alloc()
#define free_percpu(p) mem_free(p)
#define this_cpu_add(v,n) ((v)+=(n))
static void drop_mountpoint(struct fs_pin *p) { }
static void init_fs_pin(struct fs_pin *p, void (*drop)(struct fs_pin *)) { }
typedef int refcount_t;
static void refcount_set(refcount_t *p, int n) { *p=n; }
static void refcount_inc(refcount_t *p) { assert(*p>0); ++*p; }
static bool refcount_dec_and_test(refcount_t *p) { assert(*p>0); return --*p==0; }
struct ida { int used[256], count, fail; };
static struct ida mnt_id_ida, mnt_group_ida, unnamed_dev_ida;
static bool has_id(struct ida *a, int id) { for (int i=0;i<a->count;++i) if (a->used[i]==id) return true; return false; }
static int ida_alloc_range(struct ida *a, unsigned int low, unsigned int high, int flags) {
    if (a->fail) { --a->fail; return -ENOSPC; }
    assert(a->count<256); unsigned int id=low;
    while (has_id(a,id)) { if (id==high) return -ENOSPC; ++id; }
    assert(id<=high); a->used[a->count++]=id; return id;
}
static int ida_alloc(struct ida *a, int flags) { return ida_alloc_range(a,0,INT_MAX,flags); }
static int ida_alloc_min(struct ida *a, int low, int flags) { return ida_alloc_range(a,low,INT_MAX,flags); }
static void ida_free(struct ida *a, unsigned int id) {
    int i; for (i=0;i<a->count;++i) if ((unsigned int)a->used[i]==id) break;
    assert(i<a->count); a->used[i]=a->used[--a->count];
}
#define MINORBITS 20
#define MKDEV(major,minor) ((dev_t)(minor))
#define MINOR(dev) ((int)(dev))
static bool ksu, umounted;
static bool susfs_is_current_ksu_domain(void) { return ksu; }
static bool susfs_is_current_proc_umounted(void) { return umounted; }
static int hash_locked;
static void lock_mount_hash(void) { assert(!hash_locked); hash_locked=1; }
static void unlock_mount_hash(void) { assert(hash_locked); hash_locked=0; }
static unsigned int sysctl_mount_max=100000;
static struct dentry *dget(struct dentry *d) { assert(d); ++d->refs; return d; }
static struct vfsmount *mntget(struct vfsmount *m) { assert(m); ++m->refs; return m; }
struct path { struct vfsmount *mnt; struct dentry *dentry; };
static void path_put(struct path *p) { assert(p->mnt->refs>0 && p->dentry->refs>0); --p->mnt->refs; --p->dentry->refs; }
static struct hlist_head lookup_head;
static struct hlist_head *m_hash(struct vfsmount *m, struct dentry *d) { return &lookup_head; }
#define DEFINE_STATIC_KEY_FALSE(n) int n
#define static_branch_unlikely(p) (*(p))
#define static_branch_enable(p) (*(p)=1)
#define static_branch_disable(p) (*(p)=0)
static bool copy_fault;
static int copy_from_user(void *d, const void *s, size_t n) { if (copy_fault) return 1; memcpy(d,s,n); return 0; }
static int copy_to_user(void *d, const void *s, size_t n) { memcpy(d,s,n); return 0; }
static bool susfs_mnt_hide_current(void);
static bool susfs_mnt_hide_proc(void);
#include "native_mount.h"
static void *vendor_alloc(void) { if (vendor_fail) { --vendor_fail; return NULL; } int *p=mem_alloc(sizeof(int)); *p=42; return p; }
static void *vendor_clone(void *old) {
    assert(old && *(int *)old==43); void *p=vendor_alloc(); if (p) *(int *)p=*(int *)old; return p;
}
static void vendor_update(void *p, struct fs_context *fc) { assert(*(int *)p==42); *(int *)p=43; ++vendor_updates; }
static struct super_operations ops = { .clone_mnt_data=vendor_clone, .update_mnt_data=vendor_update };
static struct file_system_type type = { .alloc_mnt_data=vendor_alloc };
static struct super_block sb;
static struct dentry root;
static struct mount *create(void) {
    struct fs_context fc={ .root=&root, .source="vendor", .fs_type=&type, .sb_flags=SB_KERNMOUNT };
    struct vfsmount *v=vfs_create_mount(&fc); return IS_ERR(v) ? (void *)v : real_mount(v);
}
static void destroy(struct mount *m) {
    assert(m && !IS_ERR(m)); assert(m->mnt.mnt_root->refs>0 && sb.s_active>0);
    --m->mnt.mnt_root->refs; --sb.s_active;
    list_del_init(&m->mnt_instance); list_del_init(&m->mnt_share); list_del_init(&m->mnt_slave); list_del_init(&m->mnt_expire);
    mnt_free_id(m); free_vfsmnt(m);
}
static void zero_resources(void) {
    assert(mem_live==0 && mnt_id_ida.count==0 && mnt_group_ida.count==0 && unnamed_dev_ida.count==0);
    assert(sb.s_active==0 && root.refs==0 && !hash_locked);
}
static void test_native_failures(void) {
    int *faults[]={&cache_fail,&name_fail,&vendor_fail,
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
        &owner_fail,&mnt_id_ida.fail,
#endif
#ifdef CONFIG_SMP
        &pcp_fail,
#endif
    };
    for (unsigned int i=0;i<sizeof(faults)/sizeof(*faults);++i) { *faults[i]=1; assert(IS_ERR(create())); zero_resources(); }
    struct fs_context fc={0}; assert(PTR_ERR(vfs_create_mount(&fc))==-EINVAL); zero_resources();
    struct mount *m=create(); assert(!IS_ERR(m) && m->mnt.data && *(int *)m->mnt.data==43);
    assert(m->mnt.mnt_flags==MNT_INTERNAL && m->mnt_parent==m);
    assert(m->mnt.mnt_root==&root && vendor_updates>0);
    int before=mem_live, ids=mnt_id_ida.count, refs=root.refs;
    vendor_fail=1; assert(IS_ERR(clone_mnt(m,&root,0)));
    assert(mem_live==before && mnt_id_ida.count==ids && root.refs==refs);
    mnt_group_ida.fail=1; assert(IS_ERR(clone_mnt(m,&root,CL_MAKE_SHARED)));
    assert(mem_live==before && mnt_id_ida.count==ids && root.refs==refs);
    m->mnt.mnt_flags=MNT_READONLY|MNT_NOSUID|MNT_NODEV|MNT_NOEXEC|MNT_WRITE_HOLD|MNT_MARKED|MNT_INTERNAL;
    struct mount *c=clone_mnt(m,&root,CL_UNPRIVILEGED|CL_MAKE_SHARED);
    assert(!IS_ERR(c)); assert(!(c->mnt.mnt_flags&(MNT_WRITE_HOLD|MNT_MARKED|MNT_INTERNAL)));
    assert((c->mnt.mnt_flags&(MNT_LOCK_ATIME|MNT_LOCK_READONLY|MNT_LOCK_NOSUID|MNT_LOCK_NODEV|MNT_LOCK_NOEXEC|MNT_LOCKED))==
        (MNT_LOCK_ATIME|MNT_LOCK_READONLY|MNT_LOCK_NOSUID|MNT_LOCK_NODEV|MNT_LOCK_NOEXEC|MNT_LOCKED));
    assert(IS_MNT_SHARED(c) && c->mnt_group_id>=1);
    mnt_release_group_id(c); destroy(c); destroy(m); zero_resources();
}
static void test_ids(void) {
    ksu=false; struct mount *old=create(); int low=old->mnt_id;
    ksu=true; struct mount *copy=clone_mnt(old,&root,CL_COPY_MNT_NS);
    assert(!IS_ERR(copy));
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
    assert(copy->mnt_id>=DEFAULT_KSU_MNT_ID && !susfs_mount_data(copy)->susfs_hidden && susfs_mount_data(copy)->susfs_mnt_id==low);
    assert(susfs_mount_data(copy)->susfs_view_owner==susfs_mount_data(old)->susfs_id_owner && susfs_mount_data(copy)->susfs_view_owner->refs==2);
    /* A second copied namespace shares the original presentation owner. */
    struct mount *copy2=clone_mnt(copy,&root,CL_COPY_MNT_NS);
    assert(!IS_ERR(copy2) && susfs_mount_data(copy2)->susfs_mnt_id==low && susfs_mount_data(copy2)->susfs_view_owner==susfs_mount_data(copy)->susfs_view_owner);
    assert(susfs_mount_data(copy2)->susfs_view_owner->refs==3);
    int before=mem_live, ids=mnt_id_ida.count, n=susfs_mount_data(copy2)->susfs_view_owner->refs;
    vendor_fail=1; assert(IS_ERR(clone_mnt(copy2,&root,CL_COPY_MNT_NS)));
    assert(mem_live==before && mnt_id_ida.count==ids && susfs_mount_data(copy2)->susfs_view_owner->refs==n);
    ksu=false; destroy(old);
    assert(has_id(&mnt_id_ida,low)); struct mount *fresh=create(); assert(fresh->mnt_id!=low);
    destroy(copy); assert(has_id(&mnt_id_ida,low)); destroy(copy2); assert(!has_id(&mnt_id_ida,low));
    struct mount *reuse=create(); assert(reuse->mnt_id==low); destroy(reuse); destroy(fresh); zero_resources();
    ksu=true; old=create(); assert(susfs_mount_data(old)->susfs_hidden && old->mnt_id>=DEFAULT_KSU_MNT_ID);
    ksu=false; copy=clone_mnt(old,&root,0); assert(!IS_ERR(copy) && susfs_mount_data(copy)->susfs_hidden && copy->mnt_id>=DEFAULT_KSU_MNT_ID);
    int count=mnt_id_ida.count; /* Failure in required low-to-high reclassification. */
    mnt_id_ida.fail=0; owner_fail=0;
    /* Direct classification failure preserves the already owned allocation. */
    struct mount *raw=alloc_vfsmnt("failure"); assert(raw && raw->mnt_id<DEFAULT_KSU_MNT_ID);
    int raw_id=raw->mnt_id; mnt_id_ida.fail=1;
    assert(susfs_clone_mnt_id(raw,old,0)==-ENOSPC && raw->mnt_id==raw_id && has_id(&mnt_id_ida,raw_id));
    mnt_free_id(raw); free_vfsmnt(raw); assert(mnt_id_ida.count==count);
    destroy(copy); destroy(old); zero_resources();
    ksu=false; old=create(); ksu=true; copy=clone_mnt(old,&root,0);
    assert(!IS_ERR(copy) && susfs_mount_data(copy)->susfs_hidden); destroy(copy); destroy(old); zero_resources();
#else
    assert(copy->mnt_id!=low); destroy(copy); destroy(old); zero_resources();
#endif
    dev_t a,b; ksu=false; assert(!get_anon_bdev(&a) && MINOR(a)>=1); ksu=true; assert(!get_anon_bdev(&b));
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
    assert(MINOR(b)>=DEFAULT_KSU_MNT_MINOR_DEV);
#endif
    free_anon_bdev(a); free_anon_bdev(b); unnamed_dev_ida.fail=1; assert(get_anon_bdev(&a)==-EMFILE); zero_resources(); ksu=false;
}
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
static void test_views(void) {
    struct mount *visible=create(); ksu=true; struct mount *hidden=create(); ksu=false;
    hidden->mnt_parent=visible; hidden->mnt_mountpoint=&root;
    assert(susfs_mnt_hidden(&hidden->mnt) && !susfs_mnt_hidden(&visible->mnt));
    assert(susfs_mnt_nonhidden_id(&hidden->mnt)==susfs_mount_data(visible)->susfs_mnt_id);
    struct path p; int refs=root.refs; assert(!susfs_mnt_visible_root(&hidden->mnt,&p));
    assert(p.mnt==&visible->mnt && visible->mnt.refs==1 && root.refs==refs+1); path_put(&p);
    assert(visible->mnt.refs==0 && root.refs==refs);
    hidden->mnt_parent=hidden; assert(susfs_mnt_visible_root(&hidden->mnt,&p)==-ENOENT);
    hidden->mnt_parent=NULL; assert(susfs_mnt_visible_root(&hidden->mnt,&p)==-ENOENT);
    hidden->mnt_parent=hidden; sysctl_mount_max=0; assert(susfs_mnt_visible_root(&hidden->mnt,&p)==-ENOENT); sysctl_mount_max=100000;
    hidden->mnt_parent=visible; lookup_head.first=hidden; hidden->mnt_hash.next=visible;
    visible->mnt_parent=visible; visible->mnt_mountpoint=&root;
    umounted=false; assert(__lookup_mnt(&visible->mnt,&root)==hidden);
    umounted=true; assert(__lookup_mnt(&visible->mnt,&root)==visible);
    assert(susfs_mnt_parent_id(&hidden->mnt)==susfs_mount_data(visible)->susfs_mnt_id);
    /* Policy command changes only the non-SU proc view, never lookup policy. */
    umounted=false; struct st_susfs_hide_sus_mnts_for_non_su_procs req={0}; void *arg=&req;
    assert(!susfs_mnt_hide_proc()); req.enabled=true; susfs_set_hide_sus_mnts_for_non_su_procs(&arg);
    assert(req.err==0 && susfs_mnt_hide_proc() && !susfs_mnt_hide_current());
    ksu=true; assert(!susfs_mnt_hide_proc()); umounted=true; assert(susfs_mnt_hide_proc()); umounted=false; ksu=false;
    unsigned char *raw=(void *)&req; raw[offsetof(struct st_susfs_hide_sus_mnts_for_non_su_procs,enabled)]=2;
    susfs_set_hide_sus_mnts_for_non_su_procs(&arg); assert(req.err==-EINVAL && susfs_mnt_hide_proc());
    copy_fault=true; susfs_set_hide_sus_mnts_for_non_su_procs(&arg); assert(req.err==-EFAULT); copy_fault=false;
    req.enabled=false; susfs_set_hide_sus_mnts_for_non_su_procs(&arg); assert(!req.err && !susfs_mnt_hide_proc());
    lookup_head.first=NULL; destroy(hidden); destroy(visible); zero_resources();
}
#endif
int main(void) {
    sb.s_op=&ops; INIT_LIST_HEAD(&sb.s_mounts); root.d_sb=&sb;
    test_native_failures(); test_ids();
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
    test_views();
#endif
    zero_resources(); puts("native mount bodies: allocation, cloning, unwind, ID reuse, owned views passed"); return 0;
}
