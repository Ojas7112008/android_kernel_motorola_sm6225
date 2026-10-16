/* SPDX-License-Identifier: GPL-2.0 */
/* Host mocks for extracted source. Not a kernel, policy parser or MM emulator. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "susfs_selinux.h"
#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))
/* Vendor Makefile saturates the version-code sublevel at 255. */
#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)
#define KSU_COMPAT_USE_SELINUX_STATE 1
#define KSU_COMPAT_SIDTAB_AS_REFERENCE 1
#define __user
#define __exit
#define GFP_KERNEL 0
#define SECSID_NULL 0
#define SIMPLE_TRANSACTION_LIMIT 256
#define PAGE_SIZE 4096
#define VM_WRITE 1
#define VM_MAYWRITE 2
#define BUG_ON(x) assert(!(x))
#define SECCLASS_PROCESS 1
#define SECCLASS_SECURITY 2
#define SECCLASS_KEY 3
#define SECINITSID_SECURITY 4
#define PROCESS__SETEXEC 1
#define PROCESS__SETFSCREATE 2
#define PROCESS__SETKEYCREATE 3
#define PROCESS__SETSOCKCREATE 4
#define PROCESS__SETCURRENT 5
#define PROCESS__DYNTRANSITION 6
#define PROCESS__PTRACE 7
#define KEY__CREATE 8
#define SECURITY__CHECK_CONTEXT 9
#define SECURITY__COMPUTE_AV 10
#define KSU_FEATURE_SELINUX_HIDE 1
#define GFP_ATOMIC 0
#define AUDIT_SELINUX_ERR 0
#define pr_err(...) ((void)0)
#define pr_info(...) ((void)0)
typedef int gfp_t;
typedef int rwlock_t;
typedef long loff_t;
struct page { int refs; int payload; };
struct policydb { int id; };
struct sidtab { int id; };
struct context { char *resource; };
struct selinux_kernel_status { int payload; };
struct selinux_ss {
    struct policydb policydb;
    struct sidtab *sidtab;
    rwlock_t policy_rwlock;
    pthread_mutex_t status_lock;
};
struct selinux_state { struct selinux_ss *ss; };
struct av_decision { u32 allowed, auditallow, auditdeny, seqno, flags; };
struct task_security_struct { u32 sid, osid, exec_sid, create_sid, keycreate_sid, sockcreate_sid; };
struct cred { struct task_security_struct sec; };
struct audit_buffer { int unused; };
struct super_block { void *s_fs_info; };
struct inode { struct super_block *i_sb; };
struct file { void *private_data; struct inode *inode; };
struct selinux_fs_info { struct selinux_state *state; };
struct vm_area_struct { unsigned long vm_end, vm_start, vm_pgoff, vm_flags, vm_page_prot; };
static struct sidtab live_table = {77}, reloaded_table = {88};
static struct selinux_ss ss = {.policydb = {77}, .sidtab = &live_table,
                               .status_lock = PTHREAD_MUTEX_INITIALIZER};
static struct selinux_state selinux_state = {&ss};
static pthread_mutex_t selinux_hide_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool ksu_selinux_hide_running, ksu_selinux_hide_enabled;
static struct policydb *backup_policydb;
static struct sidtab *backup_sidtab;
static struct page *fake_status, *real_status;
static pthread_mutex_t *ksu_selinux_status_lock = &ss.status_lock;
#ifdef KSU_COMPAT_USE_STATIC_KEY
static struct { bool enabled; } fake_status_initialize_key;
#define static_branch_unlikely(k) ((k)->enabled)
#else
static bool fake_status_initialize_key = true;
#endif
static _Thread_local bool hide_locked, policy_locked;
static unsigned uid = 10000;
static int context_error, live_error, saved_insert_error, live_insert_error;
static int allocation_failure, allocations, contexts_alive, parser_calls;
static u32 deny_permission;
static int permissions, ordinary_calls, commit_count, abort_count;
static int prepare_failure, bounded_error, canonical_error;
static bool single_threaded = true, reload_before_policy_lock, canonical_long;
static u32 tracer_sid;
static struct cred committed = {.sec = {.sid = 42}};
static pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t barrier_cond = PTHREAD_COND_INITIALIZER;
static bool pause_reader, reader_entered, resume_reader, writer_attempted, writer_finished;

static void mutex_lock(pthread_mutex_t *m)
{
    assert(!pthread_mutex_lock(m));
    if (m == &selinux_hide_mutex) { assert(!hide_locked); hide_locked = true; }
}
static void mutex_unlock(pthread_mutex_t *m)
{
    if (m == &selinux_hide_mutex) { assert(hide_locked); hide_locked = false; }
    assert(!pthread_mutex_unlock(m));
}
static void read_lock(rwlock_t *lock)
{
    assert(!policy_locked);
    if (reload_before_policy_lock) { ss.sidtab = &reloaded_table; reload_before_policy_lock = false; }
    policy_locked = true;
}
static void read_unlock(rwlock_t *lock) { assert(policy_locked); policy_locked = false; }
static unsigned current_uid(void) { return uid; }
static unsigned ksu_get_uid_t(unsigned v) { return v; }
static void *kzalloc(size_t n, int flags) { return calloc(1,n); }
static void kfree(void *p) { free(p); }
static char *kmemdup_nul(const char *s, size_t n, int flags)
{
    allocations++;
    if (pause_reader) {
        assert(hide_locked && !policy_locked);
        pthread_mutex_lock(&barrier_mutex);
        reader_entered = true;
        pthread_cond_broadcast(&barrier_cond);
        while (!resume_reader) pthread_cond_wait(&barrier_cond, &barrier_mutex);
        pthread_mutex_unlock(&barrier_mutex);
    }
    if (allocation_failure == allocations) return NULL;
    char *p = calloc(1, n+1);
    assert(p); memcpy(p,s,n); return p;
}
static void context_destroy(struct context *c)
{
    if (c->resource) { free(c->resource); c->resource = NULL; contexts_alive--; }
}
static int string_to_context_struct(struct policydb *pol, struct policydb *orig_pol,
                                    struct sidtab *tab, struct sidtab *orig_tab,
                                    char *s, char *orig_s, u32 n, struct context *ctx,
                                    struct context *orig_ctx, u32 def_sid, int *orig_rc)
{
    assert(hide_locked && policy_locked);
    assert(pol == backup_policydb && tab == backup_sidtab);
    assert(orig_pol == &ss.policydb && orig_tab == ss.sidtab);
    parser_calls++;
    ctx->resource = strdup(s); orig_ctx->resource = strdup(orig_s); contexts_alive += 2;
    *orig_rc = live_error;
    if (context_error) context_destroy(ctx);
    if (live_error) context_destroy(orig_ctx);
    return context_error;
}
static int sidtab_context_to_sid(struct sidtab *tab, struct context *ctx, u32 *sid)
{
    assert(hide_locked && policy_locked && ctx->resource);
    if (tab == backup_sidtab) { if (saved_insert_error) return saved_insert_error; *sid = 11; }
    else { assert(tab == ss.sidtab); if (live_insert_error) return live_insert_error; *sid = 77; }
    return 0;
}
static int ksu_security_sid_to_context(struct policydb *pol, struct sidtab *tab,
                                      u32 sid, char **canon, u32 *len)
{
    assert(hide_locked && pol == backup_policydb && tab == backup_sidtab && sid == 11);
    if (canonical_long) {
        *len = SIMPLE_TRANSACTION_LIMIT + 1;
        *canon = calloc(1,*len);
    } else {
        *canon = strdup("saved:context"); *len = strlen(*canon)+1;
    }
    assert(*canon);
    return canonical_error;
}
static void ksu_security_compute_av_user(struct policydb *pol, struct sidtab *tab,
                                         u32 source, u32 target, u16 tclass, struct av_decision *avd)
{
    assert(hide_locked && pol == backup_policydb && tab == backup_sidtab);
    assert(source == 11 && target == 11);
    *avd = (struct av_decision){.allowed = tclass ? 0x12 : 0, .auditdeny = 0xff, .seqno = 1};
}
static struct page *new_page(int payload)
{
    struct page *p = malloc(sizeof(*p)); assert(p); *p = (struct page){1,payload}; return p;
}
static void *page_address(struct page *p) { assert(p->refs > 0); return &p->payload; }
static void get_page(struct page *p) { assert(p && p->refs > 0); p->refs++; }
static void put_page(struct page *p) { assert(p && p->refs > 0); if (!--p->refs) free(p); }
#define __free_page(p) put_page(p)
static void initialize_fake_status(void)
{
    mutex_lock(&ss.status_lock);
    if (!fake_status && real_status) fake_status = new_page(real_status->payload);
    mutex_unlock(&ss.status_lock);
}
static int ksu_selinux_hide_enable(void) { assert(hide_locked); return 0; }
static void sidtab_destroy(struct sidtab *tab) { assert(hide_locked); }
static void ksu_destroy_policydb(struct policydb *pol) { assert(hide_locked); }
static void ksu_selinux_hide_disable(void) { assert(hide_locked); }
static void ksu_unregister_feature_handler(int id) {}
#ifdef CONFIG_KSU_SUSFS
#include "bakasu_paired_parser.h"
#include "bakasu_providers.h"
#endif

static struct inode *file_inode(struct file *f) { return f->inode; }
static u32 current_sid(void) { return committed.sec.sid; }
static int avc_has_perm(struct selinux_state *state, u32 source, u32 target, u16 cls, u32 perm, void *audit)
{
    permissions++; return perm == deny_permission ? -EACCES : 0;
}
static int security_context_to_sid(struct selinux_state *state, const char *s, u32 n, u32 *sid, int flags)
{
    ordinary_calls++; *sid = 66; return 0;
}
static int security_context_str_to_sid(struct selinux_state *state, const char *s, u32 *sid, int flags)
{ return security_context_to_sid(state,s,strlen(s),sid,flags); }
static int security_context_to_sid_force(struct selinux_state *state, const char *s, u32 n, u32 *sid)
{ return security_context_to_sid(state,s,n,sid,0); }
static int security_sid_to_context(struct selinux_state *state, u32 sid, char **canon, u32 *len)
{ ordinary_calls++; *canon = strdup("live:context"); *len = strlen(*canon)+1; return 0; }
static void security_compute_av_user(struct selinux_state *state, u32 s, u32 t, u16 cls, struct av_decision *avd)
{ ordinary_calls++; *avd = (struct av_decision){.allowed = 0x34, .seqno = 5}; }
static struct cred *prepare_creds(void)
{ if (prepare_failure) return NULL; struct cred *c = malloc(sizeof(*c)); assert(c); *c = committed; return c; }
static struct task_security_struct *selinux_cred(struct cred *c) { return &c->sec; }
static void commit_creds(struct cred *c) { committed = *c; commit_count++; free(c); }
static void abort_creds(struct cred *c) { abort_count++; free(c); }
static bool current_is_single_threaded(void) { return single_threaded; }
static int security_bounded_transition(struct selinux_state *s, u32 src, u32 dst) { return bounded_error; }
static u32 ptrace_parent_sid(void) { return tracer_sid; }
static bool has_cap_mac_admin(bool audit) { return false; }
static void *audit_context(void) { return NULL; }
static struct audit_buffer *audit_log_start(void *a, int flags, int type) { return NULL; }
static void audit_log_format(struct audit_buffer *a, const char *fmt, ...) {}
static void audit_log_n_untrustedstring(struct audit_buffer *a, void *v, size_t n) {}
static void audit_log_end(struct audit_buffer *a) {}
static int scnprintf(char *s, size_t n, const char *fmt, ...)
{ va_list ap; va_start(ap,fmt); int v = vsnprintf(s,n,fmt,ap); va_end(ap); return v >= (int)n ? (int)n-1 : v; }
static struct page *selinux_kernel_status_page(struct selinux_state *state) { return real_status; }
static ssize_t simple_read_from_buffer(char *buf, size_t count, loff_t *pos, const void *data, size_t n)
{ if (*pos >= (loff_t)n) return 0; n -= *pos; if (count < n) n = count; memcpy(buf,(const char *)data+*pos,n); *pos += n; return n; }
static unsigned long page_to_pfn(struct page *p) { assert(p->refs > 0); return (unsigned long)p; }
static int remap_pfn_range(struct vm_area_struct *v, unsigned long start, unsigned long pfn, unsigned long size, unsigned long prot)
{ assert(pfn && size == PAGE_SIZE); return 0; }
#include "kernel_callers.h"

static struct selinux_fs_info fsi = {&selinux_state};
static struct super_block sb = {&fsi};
static struct inode inode = {&sb};
static struct file file = {.inode = &inode};
static void reset(void)
{
    assert(!contexts_alive && !hide_locked && !policy_locked);
    uid = 10000; context_error = live_error = saved_insert_error = live_insert_error = 0;
    allocations = allocation_failure = parser_calls = 0;
    deny_permission = permissions = ordinary_calls = commit_count = abort_count = 0;
    prepare_failure = bounded_error = canonical_error = 0;
    single_threaded = true; canonical_long = false; tracer_sid = 0; committed.sec.sid = 42;
    ksu_selinux_hide_running = true; ss.sidtab = &live_table;
}
static int set_attr(const char *name)
{ char value[] = "context\n"; return selinux_setprocattr(name,value,strlen(value)); }

static void test_callers(void)
{
    reset();
    assert(set_attr("current") == 7);
#ifdef CONFIG_KSU_SUSFS
    assert(committed.sec.sid == 77 && parser_calls == 1 && !ordinary_calls);
#else
    assert(committed.sec.sid == 66 && ordinary_calls == 1);
#endif
    assert(commit_count == 1 && permissions == 2);
    const char *attrs[] = {"exec","fscreate","keycreate","sockcreate"};
    for (unsigned i=0; i<sizeof(attrs)/sizeof(attrs[0]); ++i) {
        reset(); assert(set_attr(attrs[i]) == 7 && ordinary_calls == 1 && !parser_calls);
    }
    reset(); deny_permission = PROCESS__SETCURRENT;
    assert(set_attr("current") == -EACCES && !parser_calls && !ordinary_calls && !commit_count);
    reset(); deny_permission = PROCESS__DYNTRANSITION;
    assert(set_attr("current") == -EACCES && abort_count == 1 && !commit_count && committed.sec.sid == 42);
    reset(); tracer_sid = 123; deny_permission = PROCESS__PTRACE;
    assert(set_attr("current") == -EACCES && abort_count == 1 && !commit_count);
    reset(); single_threaded = false; bounded_error = -EPERM;
    assert(set_attr("current") == -EPERM && abort_count == 1 && !commit_count);
    reset(); prepare_failure = 1;
    assert(set_attr("current") == -ENOMEM && !commit_count && !abort_count);
    reset(); uid = 9999;
    assert(set_attr("current") == 7 && ordinary_calls == 1 && committed.sec.sid == 66);
    reset(); ksu_selinux_hide_running = false;
    assert(set_attr("current") == 7 && ordinary_calls == 1 && committed.sec.sid == 66);
    reset(); char empty[] = "\n";
    assert(selinux_setprocattr("current",empty,1) == -EINVAL && !parser_calls && !commit_count);
    reset(); char buf[SIMPLE_TRANSACTION_LIMIT] = "context";
#ifdef CONFIG_KSU_SUSFS
    assert(sel_write_context(&file,buf,7) == 14);
    assert(!strcmp(buf,"saved:context") && !ordinary_calls);
#else
    assert(sel_write_context(&file,buf,7) == 13);
    assert(!strcmp(buf,"live:context") && ordinary_calls == 2);
#endif
    reset(); strcpy(buf,"context"); deny_permission = SECURITY__CHECK_CONTEXT;
    assert(sel_write_context(&file,buf,7) == -EACCES && !parser_calls && !ordinary_calls);
    reset(); strcpy(buf,"source target 2");
    assert(sel_write_access(&file,buf,strlen(buf)) > 0);
#ifdef CONFIG_KSU_SUSFS
    assert(!strncmp(buf,"12 ",3) && parser_calls == 2 && !ordinary_calls);
#else
    assert(!strncmp(buf,"34 ",3) && ordinary_calls == 3);
#endif
    reset(); strcpy(buf,"source target 2"); deny_permission = SECURITY__COMPUTE_AV;
    assert(sel_write_access(&file,buf,strlen(buf)) == -EACCES && !parser_calls && !ordinary_calls);
    reset(); strcpy(buf,"malformed");
    assert(sel_write_access(&file,buf,strlen(buf)) == -EINVAL && !parser_calls && !ordinary_calls);
    assert(!contexts_alive);
}
#ifdef CONFIG_KSU_SUSFS
static void test_paired_policy(void)
{
    int *errors[] = {&context_error,&live_error,&saved_insert_error,&live_insert_error};
    for (unsigned i=0; i<sizeof(errors)/sizeof(errors[0]); ++i) {
        reset(); *errors[i] = -EINVAL;
        assert(set_attr("current") == -EINVAL && !commit_count && committed.sec.sid == 42);
        assert(!contexts_alive);
    }
    reset(); context_error = -EINVAL; live_error = -EACCES;
    assert(set_attr("current") == -EINVAL && !commit_count);
    for (int i=1; i<=2; i++) {
        reset(); allocation_failure = i;
        assert(set_attr("current") == -ENOMEM && !commit_count && !contexts_alive);
    }
    reset(); reload_before_policy_lock = true;
    assert(set_attr("current") == 7 && ss.sidtab == &reloaded_table);
    reset(); u32 sid = 123; int error = 123;
    assert(ksu_susfs_set_current_sid("",0,&sid,&error) && error == -EINVAL && sid == SECSID_NULL);
    reset(); live_insert_error = -ENOMEM;
    assert(ksu_susfs_set_current_sid("context",7,&sid,&error) && error == -ENOMEM && sid == SECSID_NULL);
    reset(); struct policydb *saved = backup_policydb; backup_policydb = NULL;
    assert(set_attr("current") == -EAGAIN && !ordinary_calls && !commit_count);
    backup_policydb = saved;
    reset(); uid = 0; sid = 123; error = 123;
    assert(!ksu_susfs_set_current_sid("context",7,&sid,&error) && sid == 123 && error == 123);
    reset(); char *canon = NULL; u32 len = 0; canonical_error = -ENOMEM;
    assert(ksu_susfs_context_to_string("context",7,&canon,&len,&error));
    assert(error == -ENOMEM && !canon && !len && !contexts_alive);
    reset(); canonical_long = true; char buf[SIMPLE_TRANSACTION_LIMIT] = "context";
    assert(sel_write_context(&file,buf,7) == -ERANGE && !ordinary_calls);
    reset(); live_error = -EINVAL;
    assert(ksu_susfs_context_to_string("context",7,&canon,&len,&error));
    assert(!error && !strcmp(canon,"saved:context")); free(canon);
    reset(); struct av_decision avd;
    assert(ksu_susfs_compute_av("context","",2,&avd,&error) && error == -EINVAL && !contexts_alive);
    for (int i=0; i<3; ++i) {
        reset(); u32 saved_sid = 123, live_sid = 123; int live_rc = 123;
        if (i < 2) allocation_failure = i + 1;
        else live_insert_error = -ENOMEM;
        ksu_selinux_hide_lock();
        int rc = ksu_security_context_to_sid(NULL,NULL,backup_policydb,backup_sidtab,
                                             "context",7,&saved_sid,SECSID_NULL,0,&live_sid,&live_rc);
        ksu_selinux_hide_unlock();
        assert(live_rc == -ENOMEM && live_sid == SECSID_NULL && !contexts_alive);
        if (i < 2) assert(rc == -ENOMEM && saved_sid == SECSID_NULL);
        else assert(!rc && saved_sid == 11);
    }
}
static void *reader(void *unused)
{
    u32 sid; int error;
    assert(ksu_susfs_set_current_sid("context",7,&sid,&error) && !error && sid == 77);
    return NULL;
}
static void *writer(void *unused)
{
    pthread_mutex_lock(&barrier_mutex); writer_attempted = true;
    pthread_cond_broadcast(&barrier_cond); pthread_mutex_unlock(&barrier_mutex);
    assert(!selinux_hide_feature_set(0));
    ksu_selinux_hide_drop_backup_if_unused();
    pthread_mutex_lock(&barrier_mutex); writer_finished = true; pthread_mutex_unlock(&barrier_mutex);
    return NULL;
}
static void test_concurrent_drop(void)
{
    reset(); pause_reader = true; pthread_t r,w;
    assert(!pthread_create(&r,NULL,reader,NULL));
    pthread_mutex_lock(&barrier_mutex);
    while (!reader_entered) pthread_cond_wait(&barrier_cond,&barrier_mutex);
    pthread_mutex_unlock(&barrier_mutex);
    assert(!pthread_create(&w,NULL,writer,NULL));
    pthread_mutex_lock(&barrier_mutex);
    while (!writer_attempted) pthread_cond_wait(&barrier_cond,&barrier_mutex);
    assert(!writer_finished); resume_reader = true;
    pthread_cond_broadcast(&barrier_cond); pthread_mutex_unlock(&barrier_mutex);
    assert(!pthread_join(r,NULL) && !pthread_join(w,NULL));
    assert(writer_finished && !backup_policydb && !backup_sidtab && !contexts_alive);
    pause_reader = false;
}
#endif
static void test_status(void)
{
    reset(); real_status = new_page(33);
#ifdef CONFIG_KSU_SUSFS
#ifdef KSU_COMPAT_USE_STATIC_KEY
    fake_status_initialize_key.enabled = true;
#else
    fake_status_initialize_key = false;
#endif
#endif
    assert(!sel_open_handle_status(&inode,&file));
#ifdef CONFIG_KSU_SUSFS
    assert(file.private_data == fake_status && fake_status->refs == 2 && real_status->refs == 1);
    /* Retire BakaSU's owner while this open file still holds its reference. */
    ksu_selinux_hide_exit(); assert(!fake_status);
#else
    assert(file.private_data == real_status && real_status->refs == 1);
#endif
    int payload = 0; loff_t pos = 0;
    assert(sel_read_handle_status(&file,(char *)&payload,sizeof(payload),&pos) == sizeof(payload) && payload == 33);
    struct vm_area_struct v = {.vm_end = PAGE_SIZE, .vm_flags = VM_MAYWRITE};
    assert(!sel_mmap_handle_status(&file,&v) && !(v.vm_flags & VM_MAYWRITE));
    v.vm_flags = VM_WRITE; assert(sel_mmap_handle_status(&file,&v) == -EPERM);
    v.vm_flags = 0; v.vm_pgoff = 1; assert(sel_mmap_handle_status(&file,&v) == -EIO);
    v.vm_pgoff = 0; v.vm_end = PAGE_SIZE*2; assert(sel_mmap_handle_status(&file,&v) == -EIO);
#ifdef CONFIG_KSU_SUSFS
    assert(!sel_release_handle_status(&inode,&file) && !file.private_data);
    reset(); uid = 0; assert(!sel_open_handle_status(&inode,&file));
    assert(file.private_data == real_status && real_status->refs == 2);
    assert(!sel_release_handle_status(&inode,&file) && real_status->refs == 1);
    ksu_selinux_hide_exit();
#endif
    put_page(real_status); real_status = NULL;
    assert(sel_open_handle_status(&inode,&file) == -ENOMEM);
}
int main(void)
{
    backup_policydb = malloc(sizeof(*backup_policydb)); backup_sidtab = malloc(sizeof(*backup_sidtab));
    assert(backup_policydb && backup_sidtab);
    test_callers();
#ifdef CONFIG_KSU_SUSFS
    test_paired_policy();
#endif
    test_status();
#ifdef CONFIG_KSU_SUSFS
    test_concurrent_drop();
#else
    free(backup_policydb); free(backup_sidtab);
#endif
    assert(!contexts_alive);
    puts("PASS: permissions, credential SID, policy/error/fallback contracts, status ownership and mapping restrictions"
#ifdef CONFIG_KSU_SUSFS
         ", parser cleanup, allocation faults, reload selection and concurrent teardown"
#endif
    );
    return 0;
}
