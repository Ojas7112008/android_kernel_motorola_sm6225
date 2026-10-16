/* SPDX-License-Identifier: GPL-2.0 */
/* Actual source functions with host credential, filesystem and seccomp mocks. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "linux/types.h"
#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))
#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)
#define GFP_KERNEL 0
#define AT_FDCWD (-100)
#define ksu_is_current_proc_unprivillege susfs_is_current_proc_no_su
#define __read_mostly
#define likely(v) (v)
#define unlikely(v) (v)
#define smp_load_acquire(p) __atomic_load_n((p),__ATOMIC_ACQUIRE)
#define smp_store_release(p,v) __atomic_store_n((p),(v),__ATOMIC_RELEASE)
#define DEFINE_MUTEX(v) pthread_mutex_t v = PTHREAD_MUTEX_INITIALIZER
#define mutex_lock(p) assert(!pthread_mutex_lock(p))
#define mutex_unlock(p) assert(!pthread_mutex_unlock(p))
#define pr_info(...) ((void)0)
#define pr_warn(...) ((void)0)
#define pr_err(...) ((void)0)
#define CAP_SETUID 7
#define LSM_SETID_RES 4
#define KERNEL_SU_CONTEXT "u:r:su:s0"
#define PER_USER_RANGE 100000
#define FIRST_APPLICATION_UID 10000
#define LAST_APPLICATION_UID 19999
#define FIRST_ISOLATED_UID 99000
#define LAST_ISOLATED_UID 99999
#define WEBVIEW_ZYGOTE_UID 1053
#define TIF_KSU_DISABLE_ESCAPE_WITH_ROOT 63
#define TIF_PROC_IN_KSU_EXECVE 61
struct user_namespace { u32 max_uid, offset; };
typedef struct { u32 val; } kuid_t;
struct task_security_struct {u32 sid,create_sid,keycreate_sid,sockcreate_sid,exec_sid;};
struct cred {kuid_t uid,euid,suid,fsuid; struct user_namespace *user_ns; int kind; struct task_security_struct sec;};
struct thread_info {unsigned long flags;};
struct task_struct {struct thread_info thread_info; int pid; struct {int mode;} seccomp;};
static struct user_namespace init_user_ns={UINT32_MAX-1,0}, child_ns={65534,1000000};
static struct cred active, kernel_cred;
static struct cred *ksu_cred;
static struct task_struct task;
#define current (&task)
static struct cred *current_cred(void) { return &active; }
static struct user_namespace *current_user_ns(void) {return active.user_ns;}
static kuid_t current_uid(void) {return active.uid;}
#define ksu_get_uid_t(k) ((k).val)
#define BIT(n) (1UL << (n))
static void set_thread_flag(unsigned n) {task.thread_info.flags |= BIT(n);}
static void clear_thread_flag(unsigned n) {task.thread_info.flags &= ~BIT(n);}
static bool test_thread_flag(unsigned n) {return !!(task.thread_info.flags & BIT(n));}
static bool test_and_clear_thread_flag(unsigned n) {bool old=test_thread_flag(n); clear_thread_flag(n); return old;}
static void clear_tsk_thread_flag(struct task_struct *t,unsigned n) {t->thread_info.flags &= ~BIT(n);}
#include "vendor_thread_flags.h"
#include "linux/susfs_def.h"
#include "bakasu_uid_classifiers.h"
#ifdef CONFIG_KSU_SUSFS
#include "linux/ksu_susfs.h"
#endif
_Static_assert(sizeof(unsigned long)==8,"host models the native ARM64 flag word");
_Static_assert(sizeof(uid_t)==4,"UID ABI is 32 bits, including compat callers");
#define SUSFS_MASK (BIT(TIF_PROC_UMOUNTED)|BIT(TIF_PROC_NO_SU)|BIT(TIF_PROC_UMOUNTED_FOR_ZYGOTE_NEXT))
#define RESTRICT_MASK (SUSFS_MASK|BIT(TIF_KSU_DISABLE_ESCAPE_WITH_ROOT))
_Static_assert(!(RESTRICT_MASK & (_TIF_WORK_MASK|_TIF_SYSCALL_WORK)),"private flags cannot request native entry work");
_Static_assert(!(SUSFS_MASK & BIT(TIF_PROC_IN_KSU_EXECVE)),"exec marker must be distinct");
static bool capable=true, fail_prepare, allowed, should_umount, manager;
static int fail_set_user, fail_lsm, allocations, commits, aborts, provider_calls;
static int disables, unmounts, scheduled, fd_installs, immutable_calls;
static uid_t observed_old_uid, observed_new_uid;
static int sid_error, sid_calls, keyring_calls, fd_error;
static u32 resolved_sid=77;
static void *init_session_keyring;
static bool initialized_policy=true;
static kuid_t make_kuid(struct user_namespace *ns,uid_t uid)
{return (kuid_t){uid>ns->max_uid ? UINT32_MAX : uid+ns->offset};}
static bool uid_valid(kuid_t uid) {return uid.val!=UINT32_MAX;}
static bool uid_eq(kuid_t a,kuid_t b) {return a.val==b.val;}
static bool ns_capable_setid(struct user_namespace *ns,int cap) {return capable;}
static struct cred *prepare_creds(void)
{if(fail_prepare)return NULL; struct cred *c=malloc(sizeof(*c)); assert(c); *c=active; allocations++; return c;}
static int set_user(struct cred *new) {return fail_set_user;}
static int security_task_fix_setuid(struct cred *new,const struct cred *old,int flags)
{assert(flags==LSM_SETID_RES && old==&active); return fail_lsm;}
static int commit_creds(struct cred *new) {active=*new; free(new); allocations--; commits++; return 0;}
static void abort_creds(struct cred *new) {free(new); allocations--; aborts++;}
static struct task_security_struct *selinux_cred(struct cred *c) {return initialized_policy ? &c->sec : NULL;}
static int security_secctx_to_secid(const char *name,size_t len,u32 *sid)
{sid_calls++; assert(!strcmp(name,KERNEL_SU_CONTEXT)); *sid=resolved_sid; return sid_error;}
static void *ksu_get_session_keyring(const struct cred *c) {return (void *)1;}
static void setup_ksu_cred_session_keyring(void) {keyring_calls++;}
#ifdef CONFIG_KSU_SUSFS
#include "bakasu_credential_setup.h"
#endif
static bool is_zygote(const struct cred *c) {return c->kind==1;}
static bool is_zygote_next(const struct cred *c) {return c->kind==2;}
static bool is_init(const struct cred *c) {return c->kind==0;}
static bool ksu_is_allow_uid_for_current(uid_t uid)
{observed_old_uid=active.uid.val; observed_new_uid=uid; return allowed;}
static bool ksu_uid_should_umount(uid_t uid) {return should_umount;}
static bool ksu_is_manager_uid(uid_t uid) {return manager;}
static void ksu_mark_manager(uid_t uid) {assert(uid==observed_new_uid);}
static void ksu_set_ksud_status(uid_t uid) {immutable_calls++;}
static int ksu_install_fd(void) {fd_installs++;return fd_error;}
static void disable_seccomp(void) {disables++; task.seccomp.mode=0; clear_thread_flag(TIF_SECCOMP);}
static void ksu_clear_current_proc_unprivillege(void) {susfs_clear_current_proc_no_su();}
static void ksu_set_current_proc_unprivillege(void) {susfs_set_current_proc_no_su();}
struct work_struct {bool pending;};
static struct work_struct susfs_extra_works;
static bool work_pending(struct work_struct *w) {return w->pending;}
static bool schedule_work(struct work_struct *w) {w->pending=true; scheduled++;return true;}
static int ksu_handle_umount(uid_t old,uid_t new)
{
    assert(old==active.uid.val); unmounts++;
    if((is_appuid(new)||new==WEBVIEW_ZYGOTE_UID||is_isolated_process(new)) &&
       (should_umount||is_isolated_process(new))) {
        susfs_set_current_proc_umounted();
        if(!work_pending(&susfs_extra_works))schedule_work(&susfs_extra_works);
    }
    return 0;
}
#ifdef CONFIG_KSU_SUSFS
#include "bakasu_uid_provider.h"
#endif
#ifdef UID_EARLY_NEGATIVE
#include "kernel_early_uid_caller.h"
#else
#include "kernel_uid_caller.h"
#endif
#define SYSCALL_DEFINE3(name,t1,a1,t2,a2,t3,a3) long sys_##name(t1 a1,t2 a2,t3 a3)
typedef uint16_t old_uid_t;
#define low2highuid(uid) ((uid)==(old_uid_t)-1 ? (uid_t)-1 : (uid_t)(uid))
#include "kernel_uid16_caller.h"
static void reset(int kind,uid_t uid,bool ready)
{
    assert(!allocations);
    active=(struct cred){.uid={uid},.euid={uid},.suid={uid},.fsuid={uid},.user_ns=&init_user_ns,.kind=kind};
    kernel_cred=(struct cred){.sec={.exec_sid=9}}; ksu_cred=&kernel_cred;
    task=(struct task_struct){.pid=42,.thread_info={BIT(TIF_SECCOMP)|BIT(TIF_32BIT)},.seccomp={2}};
    capable=true; fail_prepare=false; fail_set_user=fail_lsm=0;
    allowed=manager=false; should_umount=true;
    commits=aborts=provider_calls=disables=unmounts=scheduled=fd_installs=immutable_calls=0;
    sid_error=sid_calls=keyring_calls=fd_error=0; resolved_sid=77; initialized_policy=true; init_session_keyring=NULL;
    susfs_extra_works.pending=false;
#ifdef CONFIG_KSU_SUSFS
    ksu_devon_cred_ready=ready;
#endif
}
static void no_ksu_effects(unsigned long before)
{assert(task.thread_info.flags==before && !disables && !unmounts && !scheduled && !fd_installs && !immutable_calls);}
static void test_uid_errors(void)
{
    for(int restricted=0;restricted<2;restricted++) for(int failure=0;failure<5;failure++) {
        reset(1,0,true);
        if(restricted)task.thread_info.flags|=RESTRICT_MASK;
        unsigned long before=task.thread_info.flags;
        int expected;
        if(failure==0) {active.user_ns=&child_ns; expected=-EINVAL;}
        else if(failure==1) {fail_prepare=true; expected=-ENOMEM;}
        else if(failure==2) {capable=false; expected=-EPERM;}
        else if(failure==3) {fail_set_user=-EAGAIN; expected=-EAGAIN;}
        else {fail_lsm=-EACCES; expected=-EACCES;}
        assert(__sys_setresuid(failure==0?100000:10001,10001,10001)==expected);
        assert(!commits && !active.uid.val && !allocations); no_ksu_effects(before);
    }
    reset(1,0,true); unsigned long before=task.thread_info.flags;
    assert(!__sys_setresuid((uid_t)-1,(uid_t)-1,(uid_t)-1)); no_ksu_effects(before);
    reset(1,0,true); before=task.thread_info.flags;
    assert(!__sys_setresuid(0,0,0)); no_ksu_effects(before);
    reset(1,0,true); before=task.thread_info.flags;
    assert(!__sys_setresuid((uid_t)-1,10001,(uid_t)-1)); no_ksu_effects(before);
    assert(active.uid.val==0 && active.euid.val==10001 && active.suid.val==0);
    reset(1,0,true); before=task.thread_info.flags;
    assert(!__sys_setresuid(10001,10002,10003)); no_ksu_effects(before);
    reset(1,1000000,true); active.user_ns=&child_ns; before=task.thread_info.flags;
    assert(!__sys_setresuid(10001,10001,10001)); no_ksu_effects(before);
    assert(active.uid.val==1010001);
    reset(3,10001,true); capable=false; before=task.thread_info.flags;
    assert(!__sys_setresuid(10001,10001,10001)); no_ksu_effects(before);
    reset(1,0,true); assert(!sys_setresuid16(10001,10001,10001));
    assert(active.uid.val==10001 && commits==1);
    reset(1,0,true); before=task.thread_info.flags;
    assert(!sys_setresuid16(UINT16_MAX,UINT16_MAX,UINT16_MAX)); no_ksu_effects(before);
    reset(1,0,true); fail_lsm=-EACCES; before=task.thread_info.flags;
    assert(sys_setresuid16(10001,10001,10001)==-EACCES); no_ksu_effects(before);
}
#ifdef CONFIG_KSU_SUSFS
static void test_uid_classes(void)
{
    const uid_t uids[]={10001,110001,99001,199001,WEBVIEW_ZYGOTE_UID};
    for(int kind=1;kind<=2;kind++) for(unsigned i=0;i<sizeof(uids)/sizeof(uids[0]);i++) {
        reset(kind,kind==1?0:10002,true);
        assert(!__sys_setresuid(uids[i],uids[i],uids[i]));
        bool classified_mount = kind==1 || uids[i]!=WEBVIEW_ZYGOTE_UID;
        assert(active.uid.val==uids[i] && susfs_is_current_proc_no_su());
        assert(susfs_is_current_proc_umounted()==classified_mount);
        assert(test_thread_flag(TIF_32BIT) && task.seccomp.mode==2 && !disables && scheduled==classified_mount);
        assert(susfs_is_current_proc_umounted_for_zygote_next()==(kind==2 && classified_mount));
    }
    for(int kind=1;kind<=2;kind++) {
        reset(kind,kind==1?0:10002,true); allowed=true; should_umount=false;
        task.thread_info.flags|=RESTRICT_MASK;
        assert(!__sys_setresuid(10010,10010,10010));
        assert(!susfs_is_current_proc_no_su() && susfs_is_current_proc_umounted());
        assert(test_thread_flag(TIF_KSU_DISABLE_ESCAPE_WITH_ROOT) && disables==1 && task.seccomp.mode==0);
        assert(!unmounts && !scheduled && observed_old_uid==(kind==1?0:10002));
    }
    reset(2,10002,true); allowed=true;
    assert(!__sys_setresuid(10010,10010,10010));
    assert(susfs_is_current_proc_no_su() && susfs_is_current_proc_umounted_for_zygote_next() && !disables && scheduled==1);
    reset(1,0,true); allowed=manager=true;
    assert(!__sys_setresuid(10010,10010,10010)); assert(fd_installs==1 && immutable_calls==1);
    reset(1,0,true); allowed=manager=true; fail_lsm=-EPERM;
    assert(__sys_setresuid(10010,10010,10010)==-EPERM && !fd_installs && !immutable_calls);
    reset(3,0,true); unsigned long before=task.thread_info.flags;
    assert(!__sys_setresuid(10001,10001,10001)); no_ksu_effects(before);
    reset(2,10002,true); should_umount=false;
    assert(!__sys_setresuid(10001,10001,10001)); assert(susfs_is_current_proc_no_su() && !scheduled && !unmounts);
    reset(1,0,false);
    assert(!__sys_setresuid(10001,10001,10001)); assert(susfs_is_current_proc_no_su() && !scheduled && !disables && !unmounts);
    reset(2,10002,false); task.thread_info.flags|=RESTRICT_MASK;
    before=task.thread_info.flags;
    assert(!__sys_setresuid(10001,10001,10001)); no_ksu_effects(before);
    reset(0,0,false); before=task.thread_info.flags;
    assert(!__sys_setresuid(10001,10001,10001)); no_ksu_effects(before);
}
static void *setup_thread(void *unused) {assert(!setup_ksu_cred());return NULL;}
static void test_startup(void)
{
    reset(0,0,false); ksu_cred=NULL; assert(setup_ksu_cred()==-EAGAIN && !ksu_susfs_cred_ready());
    ksu_cred=&kernel_cred; initialized_policy=false; assert(setup_ksu_cred() && !ksu_susfs_cred_ready());
    initialized_policy=true; sid_error=-EINVAL; assert(setup_ksu_cred()==-EINVAL && !ksu_susfs_cred_ready());
    sid_error=0; resolved_sid=0; assert(setup_ksu_cred()==-EINVAL && !ksu_susfs_cred_ready());
    assert(!kernel_cred.sec.sid && !keyring_calls);
    resolved_sid=77; assert(!setup_ksu_cred() && ksu_susfs_cred_ready() && kernel_cred.sec.sid==77 && keyring_calls==1);
    assert(kernel_cred.sec.exec_sid==9 && !kernel_cred.sec.create_sid && !kernel_cred.sec.keycreate_sid && !kernel_cred.sec.sockcreate_sid);
    int calls=sid_calls; sid_error=-EIO; assert(!setup_ksu_cred() && sid_calls==calls && kernel_cred.sec.sid==77);
    reset(0,0,false); pthread_t workers[8];
    for(int i=0;i<8;i++)assert(!pthread_create(&workers[i],NULL,setup_thread,NULL));
    for(int i=0;i<8;i++)assert(!pthread_join(workers[i],NULL));
    assert(ksu_susfs_cred_ready() && sid_calls==1 && keyring_calls==1);
}
struct path {int unused;};
struct user_arg_ptr {int unused;};
struct ksu_sulog_pending_event {int unused;};
static const char su_path[]="/system/bin/su", ksud_path[]="/data/adb/ksud", KSUD_PATH[]="/data/adb/ksud";
static bool daemon_exists=true;
static int escape_error, scoped_fds, pre_calls;
static void *override_creds(const struct cred *c) {assert(c==ksu_cred);return &active;}
static void revert_creds(const void *c) {assert(c==&active);}
static bool is_ksud_exists(void) {return daemon_exists;}
static int escape_with_root_profile(void) {return escape_error;}
static struct ksu_sulog_pending_event *ksu_sulog_capture_sucompat_manual(const char *name,struct user_arg_ptr argv,int flags) {return NULL;}
static struct ksu_sulog_pending_event *ksu_sulog_capture_root_execve_manual(const char *name,struct user_arg_ptr argv,int flags) {return NULL;}
static void ksu_sulog_emit_pending(struct ksu_sulog_pending_event *event,int ret,int flags) {}
static int ksu_install_su_fd(void) {scoped_fds++;return 3;}
static void ksu_handle_execveat_init(const char *name,void *envp) {pre_calls++;}
static void ksu_handle_execveat_ksud(const char *name,void *argv,void *envp,int *flags) {}
#ifdef KSU_COMPAT_USE_STATIC_KEY
static struct static_key_true {bool value;} ksu_su_compat_enabled={true},ksud_execve_key={false};
#define static_branch_unlikely(k) ((k)->value)
#else
static bool ksu_su_compat_enabled=true,ksud_execve_key=false;
#endif
#ifdef EXEC_BASELINE_NEGATIVE
#include "bakasu_baseline_exec_state.h"
#else
#include "bakasu_exec_state.h"
#endif
static void test_exec_and_fork(void)
{
    reset(3,10001,true); allowed=true; escape_error=0; scoped_fds=0;
    int fd=-100,flags=0,ret=-ENOENT; struct user_arg_ptr args={0}; char name[64];
    task.thread_info.flags|=RESTRICT_MASK|BIT(TIF_PROC_IN_KSU_EXECVE);
    struct task_struct child=task; ksu_susfs_reset_fork_state(&child);
    assert(!(child.thread_info.flags & BIT(TIF_PROC_IN_KSU_EXECVE)));
    assert((child.thread_info.flags & RESTRICT_MASK)==RESTRICT_MASK && test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    assert(child.thread_info.flags & BIT(TIF_32BIT));
    clear_thread_flag(TIF_PROC_NO_SU); strcpy(name,su_path);
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-EPERM);
    assert(!strcmp(name,su_path) && !test_thread_flag(TIF_PROC_IN_KSU_EXECVE) && !scoped_fds);
    clear_thread_flag(TIF_KSU_DISABLE_ESCAPE_WITH_ROOT);
    ksu_devon_cred_ready=false;
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-EAGAIN);
    assert(!strcmp(name,su_path) && !test_thread_flag(TIF_PROC_IN_KSU_EXECVE) && !scoped_fds);
    ksu_devon_cred_ready=true;
    assert(!ksu_handle_execve(&fd,name,&args,&args,&flags)); assert(test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    assert(ksu_handle_post_execve(&fd,name,&args,&args,&flags,&ret)==-EINVAL && !scoped_fds && !test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    strcpy(name,su_path); assert(!ksu_handle_execve(&fd,name,&args,&args,&flags)); ret=0;
    assert(!ksu_handle_post_execve(&fd,name,&args,&args,&flags,&ret) && scoped_fds==1 && !test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    assert(ksu_handle_post_execve(&fd,name,&args,&args,&flags,&ret)==-EINVAL && scoped_fds==1);
    strcpy(name,su_path); escape_error=-ENOMEM;
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-ENOMEM && !strcmp(name,su_path) && !test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    task.thread_info.flags|=BIT(TIF_PROC_IN_KSU_EXECVE); strcpy(name,"/system/bin/true");
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-EINVAL && !test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    assert((task.thread_info.flags & (SUSFS_MASK & ~BIT(TIF_PROC_NO_SU)))==(SUSFS_MASK & ~BIT(TIF_PROC_NO_SU)));
    assert(test_thread_flag(TIF_32BIT));
    escape_error=0; daemon_exists=false; strcpy(name,su_path);
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-EINVAL && !strcmp(name,su_path));
    assert(!test_thread_flag(TIF_PROC_IN_KSU_EXECVE) && scoped_fds==1);
    daemon_exists=true; flags=1;
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-EINVAL && !strcmp(name,su_path));
    flags=0; allowed=false;
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-EINVAL && !strcmp(name,su_path));
    allowed=true;
#ifdef KSU_COMPAT_USE_STATIC_KEY
    ksu_su_compat_enabled.value=false;
#else
    ksu_su_compat_enabled=false;
#endif
    assert(ksu_handle_execve(&fd,name,&args,&args,&flags)==-EINVAL && !strcmp(name,su_path));
#ifdef KSU_COMPAT_USE_STATIC_KEY
    ksu_su_compat_enabled.value=true;
#else
    ksu_su_compat_enabled=true;
#endif
    assert(!ksu_handle_execve(&fd,name,&args,&args,&flags));
    assert(ksu_handle_post_execve(&fd,name,&args,&args,&flags,NULL)==-EINVAL && scoped_fds==1);
    assert(!test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
}
#endif
int main(void)
{
#ifdef EXEC_BASELINE_NEGATIVE
    reset(3,10001,true); allowed=true; escape_error=-ENOMEM; scoped_fds=0;
    int fd=AT_FDCWD,flags=0,ret=-ENOENT; struct user_arg_ptr args={0}; char name[64];
    strcpy(name,su_path);
    assert(!ksu_handle_execve(&fd,name,&args,&args,&flags) && strcmp(name,su_path));
    assert(test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    assert(!ksu_handle_post_execve(&fd,name,&args,&args,&flags,&ret) && scoped_fds==1);
    assert(test_thread_flag(TIF_PROC_IN_KSU_EXECVE));
    puts("EXPECTED NEGATIVE: pinned exec ignores escape failure and grants a scoped FD after failed exec, leaving its marker set");
    return 0;
#elif defined(UID_EARLY_NEGATIVE)
    reset(1,0,true); fail_lsm=-EACCES;
    assert(__sys_setresuid(10001,10001,10001)==-EACCES && !active.uid.val);
    assert(susfs_is_current_proc_no_su() && susfs_is_current_proc_umounted() && scheduled==1);
    puts("EXPECTED NEGATIVE: legacy early hook changes SUSFS state and schedules work despite a failed UID syscall");
    return 0;
#else
    test_uid_errors();
#ifdef CONFIG_KSU_SUSFS
    test_uid_classes(); test_startup(); test_exec_and_fork();
#endif
    assert(!allocations);
    puts("PASS: validated UID errors/partial/no-op/userns/compat paths"
#ifdef CONFIG_KSU_SUSFS
         "; Zygote classes and readiness; serialized credential setup; fork restrictions and one-shot exec marker"
#endif
    );
    return 0;
#endif
}
