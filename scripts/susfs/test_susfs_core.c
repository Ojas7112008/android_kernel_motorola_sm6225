/* SPDX-License-Identifier: GPL-2.0 */
/* Whole staged core and pinned dispatcher; VFS/fsnotify/workqueue/user-copy models. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <linux/types.h>
#define __user
#define pr_info(...) ((void)0)
#define likely(v) (v)
#define unlikely(v) (v)
#define __must_check __attribute__((warn_unused_result))
#include "native_overflow.h"
#define HZ 100
#define GFP_KERNEL 0
#define LOOKUP_FOLLOW 1
#define LOOKUP_DIRECTORY 2
#define ERR_PTR(n) ((void *)(intptr_t)(n))
#define PTR_ERR(p) ((int)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define WARN_ON_ONCE(x) (x)
#define ATOMIC_INIT(n) (n)
typedef atomic_int atomic_t;
#define atomic_read(p) atomic_load(p)
#define atomic_set(p,n) atomic_store(p,n)
#define atomic_xchg(p,n) atomic_exchange(p,n)
static int atomic_cmpxchg(atomic_t *p, int old, int value) { atomic_compare_exchange_strong(p,&old,value); return old; }
static _Thread_local int in_srcu;
#define DEFINE_MUTEX(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
static void mutex_lock(pthread_mutex_t *m) { assert(!in_srcu); assert(!pthread_mutex_lock(m)); }
static void mutex_unlock(pthread_mutex_t *m) { assert(!pthread_mutex_unlock(m)); }
struct static_key { atomic_bool enabled; };
#define DEFINE_STATIC_KEY_TRUE(name) struct static_key name = {true}
#define static_branch_likely(p) atomic_load(&(p)->enabled)
#define static_key_enabled(p) static_branch_likely(p)
static int key_changes;
static void static_branch_enable(struct static_key *key) { assert(!in_srcu); atomic_store(&key->enabled,true); key_changes++; }
static void static_branch_disable(struct static_key *key) { assert(!in_srcu); atomic_store(&key->enabled,false); key_changes++; }
struct task_struct { int pid; } task = {1}, *current = &task;
struct kuid { uid_t val; };
static uid_t real_uid;
static struct kuid current_uid(void) { return (struct kuid){real_uid}; }
static int test_thread_flag(int bit) { return 0; }
static void set_thread_flag(int bit) {}
static void clear_thread_flag(int bit) {}
static void clear_tsk_thread_flag(struct task_struct *t, int bit) {}
static uid_t ksu_get_uid_t(struct kuid uid) { return uid.val; }
struct cred { int tag; } root_cred = {9}, ordinary_cred = {1};
struct cred *ksu_cred = &root_cred;
static _Thread_local const struct cred *active_cred = &ordinary_cred;
static bool cred_ready;
bool ksu_susfs_cred_ready(void) { return cred_ready; }
static const struct cred *override_creds(const struct cred *cred) { assert(!in_srcu && cred == &root_cred); const struct cred *saved=active_cred; active_cred=cred; return saved; }
static void revert_creds(const struct cred *cred) { assert(active_cred == &root_cred); active_cred=cred; }
struct inode { unsigned int i_mode; int refs; } parent_inode, android_inode;
struct dentry { struct inode *inode; } parent_dentry, android_dentry;
struct path { void *mnt; struct dentry *dentry; };
static int parent_error, android_error, live_paths, path_calls;
static bool pinned_watch_model, event_during_add;
static int kern_path(const char *name, int flags, struct path *out) {
    assert(!in_srcu && active_cred == &root_cred);
    if(!pinned_watch_model) assert(flags & LOOKUP_DIRECTORY);
    path_calls++;
    bool is_parent = !strcmp(name,"/data/media/0"); assert(is_parent || !strcmp(name,"/data/media/0/Android"));
    int ret=is_parent ? parent_error : android_error; if(ret) return ret;
    *out=(struct path){(void *)1,is_parent ? &parent_dentry : &android_dentry};
    if(out->dentry->inode) out->dentry->inode->refs++;
    live_paths++; return 0;
}
static struct inode *d_backing_inode(struct dentry *dentry) { return dentry->inode; }
static void ihold(struct inode *inode) { inode->refs++; }
static void iput(struct inode *inode) { assert(--inode->refs>=1); }
#define xchg(p,v) __atomic_exchange_n(p,v,__ATOMIC_SEQ_CST)
static void path_put(struct path *path) { assert(!in_srcu && live_paths > 0); if(path->dentry->inode) assert(--path->dentry->inode->refs >= 1); live_paths--; }
static int fail_alloc, alloc_calls, live_allocations;
static void *kzalloc(size_t n, int flags) { assert(!in_srcu); if(++alloc_calls==fail_alloc) return NULL; void *p=calloc(1,n); assert(p); live_allocations++; return p; }
static void kfree(void *p) { if(p) { assert(live_allocations > 0); live_allocations--; free(p); } }
static ssize_t strscpy(char *dst, const char *src, size_t size) { size_t n=strlen(src); if(!size) return -E2BIG; size_t copy=n<size?n:size-1; memcpy(dst,src,copy); dst[copy]=0; return n<size ? (ssize_t)n : -E2BIG; }
static char *user_buffer;
static size_t user_size;
static bool user_readable, user_writable;
static size_t copy_from_user(void *dst, const void *src, size_t size) {
    uintptr_t at=(uintptr_t)src, base=(uintptr_t)user_buffer;
    if(!user_readable || !src || at<base || at-base>user_size || size>user_size-(at-base)) return size;
    memcpy(dst,src,size); return 0;
}
static size_t copy_to_user(void *dst, const void *src, size_t size) {
    uintptr_t at=(uintptr_t)dst, base=(uintptr_t)user_buffer;
    if(!user_writable || !dst || at<base || at-base>user_size || size>user_size-(at-base)) return size;
    memcpy(dst,src,size); return 0;
}
static void set_user(void *buffer, size_t size) { user_buffer=buffer; user_size=size; user_readable=user_writable=true; }
struct work_struct { void (*fn)(struct work_struct *); };
struct delayed_work { struct work_struct work; bool pending; unsigned long delay; };
#define DECLARE_WORK(name,callback) struct work_struct name = {callback}
#define DECLARE_DELAYED_WORK(name,callback) struct delayed_work name = {{callback},false,0}
static void *system_unbound_wq;
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static int queues;
static bool mod_delayed_work(void *wq, struct delayed_work *work, unsigned long delay) {
    assert(!pthread_mutex_lock(&queue_lock)); bool was=work->pending; work->pending=true; work->delay=delay; queues++;
    assert(!pthread_mutex_unlock(&queue_lock)); return was;
}
struct fsnotify_group;
struct fsnotify_mark;
struct fsnotify_event;
struct fsnotify_iter_info;
#include "native_fsnotify_ops.h"
typedef int refcount_t;
static void refcount_set(int *p,int n) { *p=n; }
static void refcount_inc(int *p) { ++*p; }
static bool refcount_dec_and_test(int *p) { assert(*p>0); return --*p==0; }
struct fsnotify_mark { int lock; refcount_t refcnt; struct fsnotify_group *group; u32 mask; bool attached; struct inode *inode; };
struct fsnotify_group { const struct fsnotify_ops *ops; refcount_t refcnt; struct fsnotify_mark *mark; atomic_t user_waits; int notification_waitq; struct fsnotify_event *overflow_event; };
#define FSNOTIFY_OBJ_ALL_TYPES_MASK 3
#define spin_lock_init(p) (*(p)=0)
static int mark_error, groups_freed, marks_freed;
static struct fsnotify_mark *reaper[64];
static unsigned int reaper_count;
static void fsnotify_final_destroy_group(struct fsnotify_group *g) { assert(!g->mark); groups_freed++; kfree(g); }
#include "native_mark_refs.h"
static struct fsnotify_group *fsnotify_alloc_group(const struct fsnotify_ops *ops) {
    assert(!in_srcu); struct fsnotify_group *g=kzalloc(sizeof(*g),GFP_KERNEL); if(!g) return ERR_PTR(-ENOMEM); g->ops=ops; g->refcnt=1; return g;
}
static void fsnotify_put_mark(struct fsnotify_mark *mark) {
    if(!refcount_dec_and_test(&mark->refcnt)) return;
    assert(!mark->attached);
    if(mark->inode) { assert(reaper_count<64); reaper[reaper_count++]=mark; }
    else fsnotify_final_mark_destroy(mark);
}
static int fsnotify_add_inode_mark(struct fsnotify_mark *m, struct inode *inode, int dup) {
    if(mark_error) return mark_error;
    assert(m->refcnt==1 && !m->group->mark && inode);
    m->attached=true; m->inode=inode; inode->refs++; m->group->mark=m; refcount_inc(&m->refcnt);
    if(event_during_add) {
        event_during_add=false; android_error=0; in_srcu=1;
        assert(!m->group->ops->handle_event(m->group,inode,FS_CREATE|FS_ISDIR,NULL,0,(const unsigned char *)"Android",0,NULL));
        in_srcu=0;
    }
    return 0;
}
static void fsnotify_group_stop_queueing(struct fsnotify_group *g) { assert(!in_srcu); }
static void fsnotify_clear_marks_by_group(struct fsnotify_group *g, int types) {
    struct fsnotify_mark *m=g->mark;
    if(!m) return;
    g->mark=NULL; m->attached=false; assert(--m->inode->refs>=1);
    if(g->ops->freeing_mark) g->ops->freeing_mark(m,g);
    fsnotify_put_mark(m);
}
#define wait_event(queue,condition) assert(condition)
static void fsnotify_wait_marks_destroyed(void) {
    assert(!in_srcu);
    while(reaper_count) fsnotify_final_mark_destroy(reaper[--reaper_count]);
}
static void fsnotify_flush_notify(struct fsnotify_group *group) {}
#include "native_destroy_group.h"
#include "core_under_test.h"
#include "pinned_dispatch.h"
#include "reboot_magic.h"
#define pr_err(...) ((void)0)
static int ksu_install_fd(void) { assert(!"install route outside core fixture"); return -ENOMEM; }
static void ksu_close_fd(int fd) { assert(!"unexpected close"); }
#include "pinned_transport.h"
#define SUSFS_LOGE(...) ((void)0)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include "pinned_features_negative.h"
#include "pinned_watch_negative.h"
#pragma clang diagnostic pop
static void reset(void) {
    assert(!live_allocations && !live_paths && !reaper_count);
    parent_inode=(struct inode){S_IFDIR,1}; android_inode=(struct inode){S_IFDIR,1};
    parent_dentry.inode=&parent_inode; android_dentry.inode=&android_inode;
    pinned_watch_model=event_during_add=false; parent_error=0; android_error=-ENOKEY; mark_error=fail_alloc=alloc_calls=path_calls=queues=0;
    cred_ready=true; ksu_cred=&root_cred; active_cred=&ordinary_cred; real_uid=0;
    groups_freed=marks_freed=key_changes=0;
    atomic_set(&susfs_sdcard_started,0); atomic_set(&susfs_sdcard_done,0); atomic_set(&susfs_sdcard_watch_lost,0);
    atomic_store(&susfs_is_sdcard_android_data_not_decrypted.enabled,true);
    assert(!susfs_sdcard_group && !susfs_sdcard_mark && !susfs_sdcard_path.mnt);
    susfs_sdcard_work.pending=false;
}
static void drain(void) {
    assert(!pthread_mutex_lock(&queue_lock)); assert(susfs_sdcard_work.pending); susfs_sdcard_work.pending=false;
    assert(!pthread_mutex_unlock(&queue_lock)); susfs_sdcard_work.work.fn(&susfs_sdcard_work.work);
    assert(active_cred==&ordinary_cred);
}
static void stop_fixture(void) {
    atomic_set(&susfs_sdcard_done,1); susfs_sdcard_release_watch(); fsnotify_wait_marks_destroyed();
    susfs_sdcard_work.pending=false;
    assert(!live_allocations && !live_paths && parent_inode.refs==1 && android_inode.refs==1);
}
static void event(u32 mask,const char *name) {
    in_srcu=1; assert(!susfs_sdcard_ops.handle_event(susfs_sdcard_group,&parent_inode,mask,NULL,0,(const unsigned char *)name,0,NULL)); in_srcu=0;
}
static void monitor_tests(void) {
    reset(); cred_ready=false; susfs_start_sdcard_monitor_fn(); susfs_start_sdcard_monitor_fn(); assert(queues==1); drain();
    assert(!path_calls && susfs_sdcard_work.pending && susfs_sdcard_work.delay==5*HZ && !key_changes);
    cred_ready=true; ksu_cred=NULL; drain(); assert(!path_calls && !key_changes); ksu_cred=&root_cred;
    drain(); assert(live_allocations==2 && live_paths==1 && susfs_sdcard_mark->refcnt==2 && parent_inode.refs==3);
    assert(susfs_sdcard_ops.free_mark && susfs_sdcard_ops.freeing_mark);
    int n=queues; event(FS_OPEN|FS_ISDIR,"Android2"); event(FS_OPEN|FS_ISDIR,NULL); event(FS_OPEN,"Android"); event(FS_OPEN_PERM|FS_ISDIR,"Android"); assert(queues==n);
    event(FS_OPEN|FS_ISDIR,"Android"); assert(queues==n+1 && !key_changes);
    /* A name event alone cannot open the gate while the lookup still fails. */
    drain(); assert(!atomic_read(&susfs_sdcard_done) && !key_changes && live_allocations==2);
    for(int i=0;i<50;i++) drain(); assert(live_allocations==2 && live_paths==1);
    android_error=0; drain(); assert(atomic_read(&susfs_sdcard_done) && !static_branch_likely(&susfs_is_sdcard_android_data_not_decrypted));
    assert(key_changes==1 && groups_freed==1 && !live_allocations && !live_paths);
    n=queues; event(FS_OPEN|FS_ISDIR,"Android"); susfs_start_sdcard_monitor_fn(); assert(queues==n);
    susfs_sdcard_worker(NULL); assert(key_changes==1); stop_fixture();
    reset(); android_error=0; susfs_start_sdcard_monitor_fn(); drain(); assert(key_changes==1 && !live_allocations && !live_paths); stop_fixture();
    reset(); susfs_start_sdcard_monitor_fn(); drain();
    /* Detachment callback leaves the owner's reference; the worker retires it. */
    fsnotify_clear_marks_by_group(susfs_sdcard_group,3); assert(susfs_sdcard_mark->refcnt==1);
    drain(); assert(live_allocations==2 && live_paths==1 && groups_freed==1); stop_fixture();
    reset(); susfs_start_sdcard_monitor_fn(); drain(); event(FS_MOVE_SELF,NULL); drain(); assert(groups_freed==1 && live_allocations==2); stop_fixture();
    reset(); event_during_add=true; susfs_start_sdcard_monitor_fn(); drain();
    assert(!key_changes && susfs_sdcard_work.pending); drain(); assert(key_changes==1 && !live_allocations && !live_paths); stop_fixture();
    int errors[]={-ENOENT,-ENOKEY,-EACCES,-EIO};
    for(size_t i=0;i<sizeof(errors)/sizeof(*errors);i++) { reset(); parent_error=errors[i]; susfs_start_sdcard_monitor_fn(); drain(); assert(!key_changes && !live_allocations && !live_paths); parent_error=0; drain(); assert(live_allocations==2); stop_fixture(); }
    for(int i=1;i<=2;i++) { reset(); fail_alloc=i; susfs_start_sdcard_monitor_fn(); drain(); assert(!key_changes && !live_allocations && !live_paths); fail_alloc=0; drain(); assert(live_allocations==2); stop_fixture(); }
    reset(); mark_error=-EEXIST; susfs_start_sdcard_monitor_fn(); drain(); assert(!live_allocations && !live_paths && groups_freed==1); mark_error=0; drain(); assert(live_allocations==2); stop_fixture();
    reset(); parent_dentry.inode=NULL; susfs_start_sdcard_monitor_fn(); drain(); assert(!live_allocations && !live_paths); parent_dentry.inode=&parent_inode; stop_fixture();
    reset(); android_error=0; android_inode.i_mode=S_IFREG; susfs_start_sdcard_monitor_fn(); drain(); assert(!key_changes && live_allocations==2); stop_fixture();
}
static void *wake_thread(void *arg) { for(int i=0;i<100;i++) { susfs_start_sdcard_monitor_fn(); event(FS_CREATE|FS_ISDIR,"Android"); } return NULL; }
static void *worker_thread(void *arg) { for(int i=0;i<20;i++) susfs_sdcard_worker(NULL); return NULL; }
static void concurrent_tests(void) {
    reset(); pthread_t threads[8];
    for(int i=0;i<8;i++) assert(!pthread_create(&threads[i],NULL,wake_thread,NULL));
    for(int i=0;i<8;i++) assert(!pthread_join(threads[i],NULL));
    drain(); assert(live_allocations==2 && live_paths==1); android_error=0;
    for(int i=0;i<8;i++) assert(!pthread_create(&threads[i],NULL,worker_thread,NULL));
    for(int i=0;i<8;i++) assert(!pthread_join(threads[i],NULL));
    assert(key_changes==1 && !live_allocations && !live_paths); stop_fixture();
}
static void call(unsigned int cmd,void *data,size_t size) { set_user(data,size); assert(!ksu_handle_sys_reboot(KSU_INSTALL_MAGIC1,SUSFS_MAGIC,cmd,&data)); }
static void command_tests(void) {
    reset(); struct st_susfs_version version; memset(&version,0xA5,sizeof(version)); call(CMD_SUSFS_SHOW_VERSION,&version,sizeof(version));
    assert(!version.err && !strcmp(version.susfs_version,"v2.3.0")); for(int i=7;i<16;i++) assert(!version.susfs_version[i]);
    struct st_susfs_variant variant; memset(&variant,0xA5,sizeof(variant)); call(CMD_SUSFS_SHOW_VARIANT,&variant,sizeof(variant)); assert(!variant.err && !strcmp(variant.susfs_variant,"NON-GKI"));
    struct st_susfs_enabled_features features; memset(&features,'X',sizeof(features)); call(CMD_SUSFS_SHOW_ENABLED_FEATURES,&features,sizeof(features)); assert(!features.err);
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
    assert(!strcmp(features.enabled_features,"CONFIG_KSU_SUSFS_ENABLE_LOG\n"));
#else
    assert(!features.enabled_features[0]);
#endif
    size_t n=strlen(features.enabled_features); for(size_t i=n;i<sizeof(features.enabled_features);i++) assert(!features.enabled_features[i]);
    fail_alloc=alloc_calls+1; features.err=126; call(CMD_SUSFS_SHOW_ENABLED_FEATURES,&features,sizeof(features)); assert(features.err==-ENOMEM && !live_allocations); fail_alloc=0;
    void *arg=&version; set_user(arg,sizeof(version)); user_readable=false; version.err=126; assert(!ksu_handle_susfs_cmd(CMD_SUSFS_SHOW_VERSION,&arg) && !version.err);
    version.err=126; user_writable=false; assert(!ksu_handle_susfs_cmd(CMD_SUSFS_SHOW_VERSION,&arg) && version.err==126); user_writable=true;
    version.err=126; real_uid=10001; assert(!ksu_handle_sys_reboot(KSU_INSTALL_MAGIC1,SUSFS_MAGIC,CMD_SUSFS_SHOW_VERSION,&arg) && version.err==126); real_uid=0;
    assert(!ksu_handle_susfs_cmd(0xFFFFFFFFU,&arg) && version.err==126);
    struct st_susfs_avc_log_spoofing avc={true,126}; call(CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING,&avc,sizeof(avc)); assert(avc.err==-EOPNOTSUPP);
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
    struct st_susfs_log log={false,126}; call(CMD_SUSFS_ENABLE_LOG,&log,sizeof(log)); assert(!log.err && !static_branch_likely(&susfs_is_log_enabled));
    log.enabled=true; call(CMD_SUSFS_ENABLE_LOG,&log,sizeof(log)); assert(!log.err && static_branch_likely(&susfs_is_log_enabled));
    ((unsigned char *)&log)[0]=2; call(CMD_SUSFS_ENABLE_LOG,&log,sizeof(log)); assert(log.err==-EINVAL && static_branch_likely(&susfs_is_log_enabled));
    ((unsigned char *)&log)[0]=0; arg=&log; set_user(arg,sizeof(log)); user_readable=false; assert(!ksu_handle_susfs_cmd(CMD_SUSFS_ENABLE_LOG,&arg) && log.err==-EFAULT && static_branch_likely(&susfs_is_log_enabled));
#else
    avc.err=126; call(CMD_SUSFS_ENABLE_LOG,&avc,sizeof(avc)); assert(avc.err==126);
#endif
    arg=(void *)(UINTPTR_MAX-1); ksu_handle_susfs_cmd(CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING,&arg);
    arg=NULL; set_user(NULL,0); ksu_handle_susfs_cmd(CMD_SUSFS_SHOW_VERSION,&arg); ksu_handle_susfs_cmd(CMD_SUSFS_SHOW_ENABLED_FEATURES,&arg); ksu_handle_susfs_cmd(CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING,&arg);
    assert(!live_allocations); susfs_init(); susfs_init(); susfs_extra_works.fn(&susfs_extra_works); stop_fixture();
}
static void pinned_watch_negative(void) {
    reset(); pinned_watch_model=true; active_cred=&root_cred;
    g=fsnotify_alloc_group(&susfs_sdcard_ops);
    g_watch=(struct watch_dir){.path="/data/media/0",.mask=FS_EVENT_ON_CHILD|FS_ISDIR|FS_OPEN_PERM};
    assert(!watch_one_dir(&g_watch));
    assert(g_watch.mark->refcnt==2 && live_allocations==2);
    susfs_sdcard_cleanup_fn(NULL);
    assert(!g && live_allocations==2 && !live_paths && g_watch.mark->refcnt==1);
    /* Recover the leaked creator reference so the test process itself is clean. */
    fsnotify_put_mark(g_watch.mark); g_watch.mark=NULL; fsnotify_wait_marks_destroyed();
    active_cred=&ordinary_cred; assert(!live_allocations && parent_inode.refs==1);
    pinned_watch_model=false;
}
int main(void) {
    monitor_tests(); concurrent_tests(); command_tests(); pinned_watch_negative();
    reset(); struct st_susfs_enabled_features info={0}; strcpy(info.enabled_features+128,"CONFIG_KSU_SUSFS_SUS_MOUNT\n"); void *arg=&info; set_user(arg,sizeof(info)); pinned_get_enabled_features(&arg);
    assert(!info.err && memmem(info.enabled_features,sizeof(info.enabled_features),"CONFIG_KSU_SUSFS_SUS_MOUNT",24)); assert(!live_allocations);
    puts("PASS: whole staged core plus actual transport/dispatch; query/log errors and scrubbed outputs; monitor readiness/fault retries, mark/path/group balance, SRCU deferral and concurrent cutoff");
    puts("EXPECTED NEGATIVE: pinned feature query echoes supplied bytes naming a feature that is compiled out");
    puts("EXPECTED NEGATIVE: actual pinned watch cleanup retains the creator mark reference and leaks its mark/group under the 4.19 ownership model");
    return 0;
}
