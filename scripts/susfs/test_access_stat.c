/* SPDX-License-Identifier: GPL-2.0 */
/* Actual kernel/provider functions, with host allocation/audit/VFS/LSM mocks. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "linux/types.h"
#undef PATH_MAX
#define PATH_MAX 128
#define GFP_KERNEL 0
#define __user
#define likely(v) (v)
#define unlikely(v) (v)
#define BUILD_BUG_ON(v) _Static_assert(!(v), "native layout assertion")
#define BUG_ON(v) assert(!(v))
#define ERR_PTR(v) ((void *)(intptr_t)(v))
#define PTR_ERR(p) ((long)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define IS_ERR_OR_NULL(p) (!(p) || IS_ERR(p))
#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_NO_AUTOMOUNT 0x800
#define AT_EMPTY_PATH 0x1000
#define KSTAT_QUERY_FLAGS 0x6000
#define STATX_BASIC_STATS 0x7ff
#define MAY_EXEC 1
#define MAY_ACCESS 0x10
#define SECURE_NO_SETUID_FIXUP 2
#define cap_clear(v) ((v)=0)
#define pr_info(...) ((void)0)
#include "lookup_flags.h"
struct user_namespace {int unused;};
typedef struct {u32 val;} kuid_t;
struct cred {kuid_t uid,gid,fsuid,fsgid; struct user_namespace *user_ns; unsigned long cap_effective,cap_permitted; int non_rcu;};
struct filename {const char *name; const char *uptr; int refcnt; void *aname; char iname[];};
#define EMBEDDED_NAME_MAX (PATH_MAX - offsetof(struct filename,iname))
struct thread_info {unsigned long flags;};
struct task_struct {struct thread_info thread_info;};
static struct task_struct task;
#define current (&task)
#define BIT(n) (1UL<<(n))
static void clear_tsk_thread_flag(struct task_struct *t,unsigned n) {t->thread_info.flags&=~BIT(n);}
static void set_thread_flag(unsigned n) {task.thread_info.flags|=BIT(n);}
static bool test_thread_flag(unsigned n) {return !!(task.thread_info.flags&BIT(n));}
static void clear_thread_flag(unsigned n) {task.thread_info.flags&=~BIT(n);}
static struct user_namespace ns;
static struct cred user_cred,priv_cred;
static struct cred *ksu_cred=&priv_cred;
static const struct cred *subjective=&user_cred;
static kuid_t current_uid(void) {return subjective->uid;}
#define ksu_get_uid_t(k) ((k).val)
#include "linux/susfs_def.h"
#include "linux/ksu_susfs.h"
static int names_live,creds_live,alloc_attempts,fail_name_at;
static int override_calls,revert_calls,daemon_checks,allow_checks;
static bool fail_cred,ready,allowed,daemon_present,secure_fixup;
static void *name_alloc(size_t size,bool clear)
{
    if(++alloc_attempts==fail_name_at)return NULL;
    void *p=clear?calloc(1,size):malloc(size);assert(p);names_live++;return p;
}
static void *__getname(void) {return name_alloc(PATH_MAX,false);}
static void __putname(const void *p) {assert(p && names_live>0);free((void *)p);names_live--;}
static void *kzalloc(size_t n,int flags) {return name_alloc(n,true);}
static void *kmalloc(size_t n,int flags) {return name_alloc(n,false);}
static void kfree(void *p) {__putname(p);}
static long strncpy_from_user(char *dst,const char *src,size_t n)
{
    if(!src || src==(const char *)1)return -EFAULT;
    size_t len=strnlen(src,n);memcpy(dst,src,len);if(len<n)dst[len]=0;return len;
}
static void putname(struct filename *name);
static struct filename *audit_refs[32];
static int audit_count;
static bool audit_enabled;
static struct filename *audit_reusename(const char *uptr)
{
    if(audit_enabled)for(int i=0;i<audit_count;i++)if(audit_refs[i]->uptr==uptr){audit_refs[i]->refcnt++;return audit_refs[i];}
    return NULL;
}
static void audit_getname(struct filename *n)
{if(audit_enabled){assert(audit_count<32);n->refcnt++;audit_refs[audit_count++]=n;}}
#include "native_names.h"
static const struct cred *override_creds(const struct cred *c)
{const struct cred *old=subjective;assert(c);subjective=c;override_calls++;return old;}
static void revert_creds(const struct cred *c) {assert(c);subjective=c;revert_calls++;}
static struct cred *prepare_creds(void)
{if(fail_cred)return NULL;struct cred *c=malloc(sizeof(*c));assert(c);*c=*subjective;creds_live++;return c;}
static void put_cred(struct cred *c) {assert(creds_live>0);free(c);creds_live--;}
static bool issecure(int bit) {return secure_fixup;}
static kuid_t make_kuid(struct user_namespace *n,u32 uid) {return (kuid_t){uid};}
static bool uid_eq(kuid_t a,kuid_t b) {return a.val==b.val;}
static bool ksu_susfs_cred_ready_impl(void) {return ready;}
#ifdef CONFIG_KSU_SUSFS
bool ksu_susfs_cred_ready(void) {return ksu_susfs_cred_ready_impl();}
#endif
static bool ksu_is_allow_uid_for_current(u32 uid) {allow_checks++;assert(uid==user_cred.uid.val);return allowed;}
static bool is_ksud_exists(void) {daemon_checks++;assert(subjective==ksu_cred && ready);return daemon_present;}
#define ksu_is_current_proc_unprivillege susfs_is_current_proc_no_su
static const char su_path[]="/system/bin/su",sh_path[]="/system/bin/sh";
#ifdef KSU_COMPAT_USE_STATIC_KEY
static struct static_key_true {bool value;} ksu_su_compat_enabled={true};
#define static_branch_unlikely(p) ((p)->value)
#else
static bool ksu_su_compat_enabled=true;
#endif
#ifdef CONFIG_KSU_SUSFS
#include "provider_names.h"
#endif
struct inode {unsigned int i_mode;};
struct dentry {struct inode *inode;};
struct vfsmount {bool readonly;};
struct path {struct dentry *dentry;struct vfsmount *mnt;};
struct nameidata {struct path root;int dfd;struct filename *name;};
struct kstat {int filled;};
static struct inode inode;
static struct dentry dentry={&inode};
static struct vfsmount mount;
static int paths_live,lookup_calls,path_releases,permission_calls,getattr_calls,lsm_calls;
static int lookup_results[8],lookup_result_count,permission_error,getattr_error,lsm_error;
static unsigned lookup_seen[8],stat_flags_seen,stat_mask_seen;
static int expected_dfd=23,expected_mode;
static bool expect_access,noexec,permission_stale_once,getattr_stale_once;
static char lookup_name[PATH_MAX];
static struct nameidata *active_nd;
static void set_nameidata(struct nameidata *nd,int dfd,struct filename *name) {assert(!active_nd);nd->dfd=dfd;nd->name=name;active_nd=nd;}
static void restore_nameidata(void) {assert(active_nd);active_nd=NULL;}
static int path_lookupat(struct nameidata *nd,unsigned flags,struct path *path)
{
    assert(nd==active_nd && nd->dfd==expected_dfd);
    if(expect_access){assert(subjective!=&user_cred && subjective!=ksu_cred);assert(uid_eq(subjective->fsuid,user_cred.uid) && uid_eq(subjective->fsgid,user_cred.gid));assert(subjective->non_rcu==1);if(!secure_fixup && user_cred.uid.val)assert(!subjective->cap_effective);}
    else assert(subjective==&user_cred);
    assert(lookup_calls<8);lookup_seen[lookup_calls]=flags;
    snprintf(lookup_name,sizeof(lookup_name),"%s",nd->name->name);
    int ret=lookup_calls<lookup_result_count?lookup_results[lookup_calls]:0;lookup_calls++;
    if(!ret){*path=(struct path){&dentry,&mount};paths_live++;}return ret;
}
static void audit_inode(struct filename *n,struct dentry *d,int parent) {assert(n->refcnt>0 && paths_live);}
#include "native_lookup.h"
static int user_path_at(int dfd,const char *name,unsigned flags,struct path *p) {return filename_lookup(dfd,getname_flags(name,flags,NULL),flags,p,NULL);}
static struct inode *d_backing_inode(struct dentry *d) {return d->inode;}
static void path_put(struct path *p) {assert(paths_live>0);paths_live--;path_releases++;}
static bool path_noexec(struct path *p) {return noexec;}
static int inode_permission2(struct vfsmount *m,struct inode *i,int mode)
{permission_calls++;assert(mode==(expected_mode|MAY_ACCESS));if(permission_stale_once && permission_calls==1)return -ESTALE;return permission_error;}
static bool special_file(unsigned mode) {return S_ISCHR(mode) || S_ISBLK(mode) || S_ISFIFO(mode) || S_ISSOCK(mode);}
static bool __mnt_is_readonly(struct vfsmount *m) {return m->readonly;}
static bool retry_estale(int error,unsigned flags) {return error==-ESTALE && !(flags&LOOKUP_REVAL);}
static int security_inode_getattr(const struct path *p) {lsm_calls++;return lsm_error;}
static int vfs_getattr_nosec_mnt_id(const struct path *p,struct kstat *stat,u32 mask,unsigned flags,u64 *mnt_id)
{getattr_calls++;stat_flags_seen=flags;stat_mask_seen=mask;if(getattr_stale_once && getattr_calls==1)return -ESTALE;if(getattr_error)return getattr_error;stat->filled=1;return 0;}
#include "native_getattr.h"
#include "kernel_access_stat.h"
#include "native_stat_wrappers.h"
static void reset(void)
{
    for(int i=0;i<audit_count;i++)putname(audit_refs[i]);audit_count=0;
    assert(!names_live && !creds_live && !paths_live && !active_nd);
    user_cred=(struct cred){.uid={10001},.gid={10002},.fsuid={0},.fsgid={0},.cap_effective=7,.cap_permitted=7,.user_ns=&ns};
    subjective=&user_cred;task.thread_info.flags=0;
    ready=allowed=daemon_present=true;fail_cred=secure_fixup=noexec=audit_enabled=false;
    fail_name_at=alloc_attempts=override_calls=revert_calls=daemon_checks=allow_checks=0;
    lookup_calls=path_releases=permission_calls=getattr_calls=lsm_calls=lookup_result_count=0;
    permission_error=getattr_error=lsm_error=0;permission_stale_once=getattr_stale_once=false;
    memset(lookup_results,0,sizeof(lookup_results));memset(lookup_seen,0,sizeof(lookup_seen));lookup_name[0]=0;
    inode.i_mode=S_IFREG|0755;mount.readonly=false;expected_dfd=23;expect_access=false;expected_mode=0;
#ifdef KSU_COMPAT_USE_STATIC_KEY
    ksu_su_compat_enabled.value=true;
#else
    ksu_su_compat_enabled=true;
#endif
}
static long query(bool access,const char *name,int bits)
{expect_access=access;expected_mode=bits;if(access)return do_faccessat(expected_dfd,name,bits);struct kstat stat={0};return vfs_statx(expected_dfd,name,bits,&stat,0x1234);}
static void balanced(void) {assert(subjective==&user_cred && override_calls==revert_calls && !creds_live && !paths_live && !active_nd);if(!audit_enabled)assert(!names_live);}
static void test_errors(void)
{
    for(int access=0;access<2;access++){
        reset();assert(query(access,NULL,access?8:0x80000000)==-EINVAL && !alloc_attempts && !lookup_calls && !daemon_checks);balanced();
        reset();assert(query(access,NULL,0)==-EFAULT && !lookup_calls && !daemon_checks);balanced();
        reset();fail_name_at=1;assert(query(access,"/a",0)==-ENOMEM && !lookup_calls && !daemon_checks);balanced();
        reset();assert(query(access,"",0)==-ENOENT && !lookup_calls);balanced();
        reset();lookup_results[0]=-ENOENT;lookup_result_count=1;assert(query(access,"/a",0)==-ENOENT);balanced();
        reset();lookup_results[0]=-EACCES;lookup_result_count=1;assert(query(access,"/a",0)==-EACCES);balanced();
        reset();char long_name[PATH_MAX+1];memset(long_name,'a',PATH_MAX);long_name[PATH_MAX]=0;
        assert(query(access,long_name,0)==-ENAMETOOLONG && !lookup_calls);balanced();
        long_name[EMBEDDED_NAME_MAX+1]=0;
        for(int fail=1;fail<=2;fail++){reset();fail_name_at=fail;assert(query(access,long_name,0)==-ENOMEM);balanced();}
        reset();assert(!query(access,long_name,0));balanced();
    }
    reset();fail_cred=true;assert(query(true,NULL,0)==-ENOMEM && !alloc_attempts && !lookup_calls);balanced();
}
static void test_permissions_flags(void)
{
    reset();noexec=true;assert(query(true,"/a",MAY_EXEC)==-EACCES && !permission_calls && path_releases==1);balanced();
    reset();permission_error=-EACCES;assert(query(true,"/a",2)==-EACCES && permission_calls==1);balanced();
    reset();mount.readonly=true;assert(query(true,"/a",2)==-EROFS);balanced();
    reset();mount.readonly=true;inode.i_mode=S_IFCHR;assert(!query(true,"/a",2));balanced();
    reset();user_cred.uid.val=0;assert(!query(true,"/a",0));balanced();
    reset();secure_fixup=true;assert(!query(true,"/a",0));balanced();
    reset();lsm_error=-EACCES;assert(query(false,"/a",0)==-EACCES && !getattr_calls);balanced();
    reset();getattr_error=-EIO;assert(query(false,"/a",0)==-EIO && lsm_calls==1);balanced();
    reset();assert(!query(false,"/a",AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT|0x2000));assert(!(lookup_seen[0]&LOOKUP_FOLLOW) && !(lookup_seen[0]&LOOKUP_AUTOMOUNT));assert(stat_flags_seen==(AT_SYMLINK_NOFOLLOW|AT_NO_AUTOMOUNT|0x2000) && stat_mask_seen==0x1234);balanced();
    reset();assert(!query(false,"",AT_EMPTY_PATH) && (lookup_seen[0]&LOOKUP_EMPTY));balanced();
}
static void test_stat_wrappers(void)
{
    reset();expected_dfd=AT_FDCWD;struct kstat stat={0};
    assert(!vfs_stat("/a",&stat) && stat.filled);
    assert((lookup_seen[0]&LOOKUP_FOLLOW) && !(lookup_seen[0]&LOOKUP_AUTOMOUNT) && stat_mask_seen==STATX_BASIC_STATS);balanced();
    reset();expected_dfd=AT_FDCWD;assert(!vfs_lstat("/a",&stat));
    assert(!(lookup_seen[0]&LOOKUP_FOLLOW) && !(lookup_seen[0]&LOOKUP_AUTOMOUNT));balanced();
    reset();assert(!vfs_fstatat(23,"",&stat,AT_EMPTY_PATH) && (lookup_seen[0]&LOOKUP_EMPTY));balanced();
}
static void test_retries(void)
{
    for(int access=0;access<2;access++){
        reset();lookup_results[0]=-ECHILD;lookup_results[1]=-ESTALE;lookup_result_count=2;
        assert(!query(access,"/a",0) && lookup_calls==3 && (lookup_seen[0]&LOOKUP_RCU) && !(lookup_seen[1]&LOOKUP_RCU) && (lookup_seen[2]&LOOKUP_REVAL));balanced();
        reset();audit_enabled=true;
#ifdef CONFIG_KSU_SUSFS
        const char *name=su_path;
#else
        const char *name="/a";
#endif
        if(access)permission_stale_once=true;else getattr_stale_once=true;
        assert(!query(access,name,0) && lookup_calls==2 && path_releases==2 && (lookup_seen[1]&LOOKUP_REVAL));
        assert(audit_count>=1 && !strcmp(audit_refs[0]->name,name));balanced();
    }
}
#ifdef CONFIG_KSU_SUSFS
static void test_redirect(void)
{
    for(int access=0;access<2;access++){
        reset();audit_enabled=true;assert(!query(access,su_path,0));assert(!strcmp(lookup_name,sh_path) && daemon_checks==1);assert(audit_count==2 && !strcmp(audit_refs[0]->name,su_path));balanced();
        for(int audited=0;audited<2;audited++){
            reset();audit_enabled=audited;fail_name_at=2;
            assert(query(access,su_path,0)==-ENOMEM && !lookup_calls);
            if(audited)assert(audit_count==1 && !strcmp(audit_refs[0]->name,su_path));balanced();
        }
        for(int gate=0;gate<5;gate++){
            reset();if(gate==0)allowed=false;if(gate==1)ready=false;if(gate==2)set_thread_flag(TIF_PROC_NO_SU);if(gate==3)daemon_present=false;
            if(gate==4){
#ifdef KSU_COMPAT_USE_STATIC_KEY
                ksu_su_compat_enabled.value=false;
#else
                ksu_su_compat_enabled=false;
#endif
            }
            assert(!query(access,su_path,0) && !strcmp(lookup_name,su_path));assert(daemon_checks==(gate==3));balanced();
        }
        reset();assert(!query(access,"s",0) && !strcmp(lookup_name,"s") && !daemon_checks);balanced();
        reset();assert(!query(access,"/system/bin/su-other",0) && !daemon_checks);balanced();
    }
    reset();int dfd=23,mode=0,flags=0;struct filename *n=NULL;
    assert(ksu_handle_stat(&dfd,NULL,&flags)==-EINVAL);assert(ksu_handle_faccessat(&dfd,&n,&mode,NULL)==-EINVAL);
    n=ERR_PTR(-EFAULT);assert(ksu_handle_stat(&dfd,&n,&flags)==-EINVAL);
}
#endif
int main(void)
{
#ifdef PROVIDER_BASELINE_NEGATIVE
    reset();audit_enabled=true;allowed=false;set_thread_flag(TIF_PROC_NO_SU);
    assert(!query(true,su_path,0) && !strcmp(lookup_name,sh_path));
    assert(!strcmp(audit_refs[0]->name,sh_path) && daemon_checks==1);balanced();reset();
    puts("EXPECTED NEGATIVE: pinned access provider redirects without caller allow/no-SU gates and mutates the original audit name");
#else
    reset();test_errors();test_permissions_flags();test_stat_wrappers();test_retries();
#ifdef CONFIG_KSU_SUSFS
    test_redirect();
#endif
    reset();puts("PASS: actual filename allocation/consumption, audit preservation, retries, native access credentials/permissions and stat LSM/flags"
#ifdef CONFIG_KSU_SUSFS
    "; checked owned-name SU redirection and readiness/allow/no-SU/static-key gates"
#endif
    );
#endif
    return 0;
}
