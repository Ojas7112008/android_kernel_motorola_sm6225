/* Actual statx/getattr bodies; explicit single-threaded VFS/LSM models. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stat_payload.h"
#include "lookup_flags.h"

#define __user
#define noinline_for_stack
#define KSTAT_QUERY_FLAGS 0x6000
#define AT_STATX_SYNC_TYPE 0x6000
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_NO_AUTOMOUNT 0x800
#define AT_EMPTY_PATH 0x1000
#define ERR_PTR(v) ((void *)(intptr_t)(v))
#define PTR_ERR(p) ((long)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define MAJOR(d) ((d) >> 20)
#define MINOR(d) ((d) & ((1U << 20)-1))
#define SYSCALL_DEFINE5(n,t1,a1,t2,a2,t3,a3,t4,a4,t5,a5) \
    static long syscall_##n(t1 a1,t2 a2,t3 a3,t4 a4,t5 a5)

struct path;
struct inode_operations {
    int (*getattr)(const struct path *, struct kstat *, u32, unsigned);
};
struct inode { struct inode_operations *i_op; };
struct dentry { struct inode *inode; };
struct vfsmount { int id, visible_id; };
struct path { struct dentry *dentry; struct vfsmount *mnt; };
struct filename { const char *name; };
static struct inode inode;
static struct dentry dentry = { &inode };
static struct vfsmount mount = { 55, 11 };
static int refs, names, lookups, path_puts, getattr_calls, lsm_calls, copy_calls;
static int lookup_error, lsm_error, fs_error, hook_error;
static unsigned seen_lookup, seen_query;
static u32 seen_mask;
static bool stale_once, hide_mount, rule, copy_fault;

static struct inode *d_backing_inode(struct dentry *d) { return d->inode; }
static void *current_user_ns(void) { return NULL; }
static unsigned from_kuid_munged(void *ns, kuid_t uid) { return uid.val; }
static unsigned from_kgid_munged(void *ns, kgid_t gid) { return gid.val; }
static int copy_to_user(void *dst, const void *src, size_t count)
{
    copy_calls++;
    if (copy_fault) return count;
    memcpy(dst, src, count);
    return 0;
}
static int security_inode_getattr(const struct path *p)
{
    assert(refs > 0 && p->mnt == &mount);
    lsm_calls++;
    return lsm_error;
}
static void generic_fillattr(struct inode *i, struct kstat *s)
{
    s->ino=99; s->mode=0100644; s->nlink=2; s->blksize=4096;
    s->size=12345; s->blocks=32; s->uid.val=10000; s->gid.val=10001;
    s->dev=(8U<<20)|7; s->rdev=(1U<<20)|3;
    s->atime=(struct timespec64){10,11}; s->mtime=(struct timespec64){12,13};
    s->ctime=(struct timespec64){14,15}; s->btime=(struct timespec64){16,17};
}
static int fs_getattr(const struct path *p, struct kstat *s, u32 mask, unsigned query)
{
    assert(refs > 0);
    getattr_calls++; seen_mask=mask; seen_query=query;
    if (stale_once && getattr_calls==1) return -ESTALE;
    if (fs_error) return fs_error;
    generic_fillattr(p->dentry->inode,s);
    return 0;
}
static bool susfs_mnt_hide_current(void) { return hide_mount; }
static int susfs_mnt_id(struct vfsmount *m) { assert(refs > 0); return m->id; }
static int susfs_mnt_nonhidden_id(struct vfsmount *m) { assert(refs > 0); return m->visible_id; }
/* Real rule ownership and the shared metadata/ID snapshot are covered by
 * test_susfs_kstat.py. Here the model checks the syscall's private transport. */
static void susfs_spoof_kstat(struct inode *i, struct kstat *s, u64 *id)
{
    if (!rule) return;
    s->ino=777;
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
    if (id) *id=23;
#endif
}
static struct filename name;
static struct filename *getname_flags(const char *s, unsigned flags, void *unused)
{
    if (!s) return ERR_PTR(-EFAULT);
    if (!*s && !(flags & LOOKUP_EMPTY)) return ERR_PTR(-ENOENT);
    assert(!names); names++; name.name=s; return &name;
}
static void putname(struct filename *n) { assert(n==&name && names==1); names--; }
static int ksu_handle_stat(int *dfd, struct filename **n, int *flags) { return hook_error; }
static int filename_lookup(int dfd, struct filename *n, unsigned flags,
                           struct path *p, void *root)
{
    assert(names==1); lookups++; seen_lookup=flags; putname(n);
    if (lookup_error) return lookup_error;
    refs++; *p=(struct path){&dentry,&mount}; return 0;
}
static int user_path_at(int dfd, const char *s, unsigned flags, struct path *p)
{
    struct filename *n=getname_flags(s,flags,NULL);
    if (IS_ERR(n)) return PTR_ERR(n);
    return filename_lookup(dfd,n,flags,p,NULL);
}
static void path_put(struct path *p) { assert(refs > 0); refs--; path_puts++; }
static bool retry_estale(int error, unsigned flags) { return error==-ESTALE && !(flags&LOOKUP_REVAL); }
#include "native_statx.h"

static struct inode_operations ops = { fs_getattr };
static void reset(void)
{
    assert(!refs && !names);
    lookups=path_puts=getattr_calls=lsm_calls=copy_calls=0;
    lookup_error=lsm_error=fs_error=hook_error=0;
    seen_lookup=seen_query=seen_mask=0;
    stale_once=hide_mount=rule=copy_fault=false;
    inode.i_op=&ops; mount=(struct vfsmount){55,11};
}
static void check_result(const struct statx *s, u64 id)
{
    assert(s->stx_size==12345 && s->stx_uid==10000 && s->stx_gid==10001);
    assert(s->stx_atime.tv_sec==10 && s->stx_atime.tv_nsec==11);
    assert(s->stx_btime.tv_sec==16 && s->stx_btime.tv_nsec==17);
    assert(s->stx_dev_major==8 && s->stx_dev_minor==7);
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
    assert((s->stx_mask&STATX_MNT_ID) && s->stx_mnt_id==id);
#else
    assert(!(s->stx_mask&STATX_MNT_ID) && !s->stx_mnt_id);
#endif
    assert(!s->__spare0[0]);
    for (unsigned i=0; i<13; i++) assert(!s->__spare2[i]);
    assert(!refs && !names);
}
static void success_cases(void)
{
    struct statx s;
    reset(); memset(&s,0xa5,sizeof(s));
    assert(!syscall_statx(-100,"/file",0,STATX_MNT_ID,&s)); check_result(&s,55);
    assert(getattr_calls==1 && lsm_calls==1 && path_puts==1 && copy_calls==1);
    assert(seen_mask==0 && seen_query==0);
    reset(); hide_mount=true;
    assert(!syscall_statx(-100,"/file",0,STATX_ALL,&s)); check_result(&s,11);
    reset(); mount.id=0;
    assert(!syscall_statx(-100,"/file",0,STATX_MNT_ID,&s)); check_result(&s,0);
    reset(); rule=true;
    assert(!syscall_statx(-100,"/file",0,STATX_ALL,&s));
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
    assert(s.stx_ino==777); check_result(&s,23);
#else
    assert(s.stx_ino==99); check_result(&s,55);
#endif
    reset(); inode.i_op->getattr=NULL;
    assert(!syscall_statx(-100,"/file",0,STATX_MNT_ID,&s)); check_result(&s,55);
    inode.i_op->getattr=fs_getattr;
    reset();
    assert(!syscall_statx(7,"",AT_EMPTY_PATH|AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT|0x2000,STATX_ALL,&s));
    assert((seen_lookup&LOOKUP_EMPTY) && !(seen_lookup&(LOOKUP_FOLLOW|LOOKUP_AUTOMOUNT)) && seen_query==0x2000);
    check_result(&s,55);
    reset(); stale_once=true;
    assert(!syscall_statx(-100,"/file",0,STATX_MNT_ID,&s)); check_result(&s,55);
    assert(lookups==2 && path_puts==2 && getattr_calls==2 && lsm_calls==2 && (seen_lookup&LOOKUP_REVAL));
    reset();
    struct { u64 before; struct kstat stat; u64 after; } native = {.before=0x12345678,.after=0xabcdef};
    assert(!vfs_statx(-100,"/file",0,&native.stat,STATX_ALL));
    assert(native.before==0x12345678 && native.after==0xabcdef && !(native.stat.result_mask&STATX_MNT_ID));
    assert(native.stat.ino==99 && !refs && !names);
}
static void error_cases(void)
{
    struct statx s;
    reset(); assert(syscall_statx(-100,"/file",0,STATX__RESERVED,&s)==-EINVAL && !lookups && !copy_calls);
    reset(); assert(syscall_statx(-100,"/file",AT_STATX_SYNC_TYPE,0,&s)==-EINVAL && !lookups);
    reset(); assert(syscall_statx(-100,"/file",0x80000000,0,&s)==-EINVAL && !lookups);
    reset(); assert(syscall_statx(-100,NULL,0,0,&s)==-EFAULT && !refs && !names);
    reset(); assert(syscall_statx(7,"",0,0,&s)==-ENOENT && !refs && !names);
    reset(); lookup_error=-ENOENT; assert(syscall_statx(-100,"/file",0,0,&s)==-ENOENT && !path_puts && !lsm_calls && !copy_calls);
    reset(); lsm_error=-EACCES; assert(syscall_statx(-100,"/file",0,0,&s)==-EACCES && !getattr_calls && path_puts==1 && !copy_calls);
    reset(); fs_error=-EIO; assert(syscall_statx(-100,"/file",0,0,&s)==-EIO && path_puts==1 && !copy_calls);
    reset(); fs_error=-ESTALE; assert(syscall_statx(-100,"/file",0,0,&s)==-ESTALE && lookups==2 && path_puts==2 && !copy_calls);
    reset(); copy_fault=true; assert(syscall_statx(-100,"/file",0,0,&s)==-EFAULT && copy_calls==1 && path_puts==1);
#ifdef CONFIG_KSU_SUSFS
    reset(); hook_error=-EPERM; assert(syscall_statx(-100,"/file",0,0,&s)==-EPERM && !names && !lookups && !copy_calls);
#endif
    assert(!refs && !names);
}
int main(void)
{
    success_cases(); error_cases();
    puts("statx transport, ABI canaries, security, errors, retries and modeled path lifetime passed");
    return 0;
}
