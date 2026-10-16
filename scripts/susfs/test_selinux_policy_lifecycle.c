/* SPDX-License-Identifier: GPL-2.0 */
/* Host ownership/locking mocks; real policy parser and rule primitives are not emulated. */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))
/* Vendor Makefile saturates the version-code sublevel at 255. */
#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)
#define KSU_COMPAT_USE_SELINUX_STATE 1
#define __user
#define GFP_KERNEL 0
#define KERNEL_SU_DOMAIN "ksu"
#define KERNEL_SU_FILE "ksu_file"
#define POLICYDB_VERSION_XPERMS_IOCTL 30
#define KSU_SEPOLICY_MAX_BATCH_SIZE (8U * 1024U * 1024U)
#define KSU_SEPOLICY_MAX_ARGS 5
#define MAX_ARG_STRINGS 1024
#define unlikely(x) (x)
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define check_mul_overflow(a,b,d) __builtin_mul_overflow(a,b,d)
#define check_add_overflow(a,b,d) __builtin_add_overflow(a,b,d)
#define pr_err(...) log_mock(__VA_ARGS__)
#define pr_info(...) log_mock(__VA_ARGS__)
static void log_mock(const char *fmt, ...) {}
struct policydb { size_t len; struct {u32 nprim;} p_types; int marker; u32 policyvers; void *resource; };
struct policy_file { void *data; size_t len; };
struct sidtab { void *resource; };
struct selinux_ss { struct policydb policydb; int policy_rwlock; struct sidtab *sidtab; };
struct selinux_state { struct selinux_ss *ss; bool initialized; };
struct selinux_fs_info { pthread_mutex_t mutex; };
struct super_block { void *s_fs_info; };
struct vfsmount { struct super_block *mnt_sb; };
struct sepol_batch_cursor { const u8 *cur, *end; };
struct sepol_data { unsigned cmd, subcmd; };
struct user_arg_ptr { int argc; const char *args[3]; };
static struct selinux_ss ss;
static struct selinux_state selinux_state = {&ss,true};
static struct selinux_fs_info fsi = {PTHREAD_MUTEX_INITIALIZER};
static struct super_block sb = {&fsi};
static struct vfsmount mount = {&sb};
static struct vfsmount *selinuxfs_mount = &mount;
static struct policydb *backup_policydb;
static struct sidtab *backup_sidtab;
static bool ksu_selinux_hide_running;
static pthread_mutex_t selinux_hide_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local bool hide_locked, fs_locked, policy_locked;
static int fault_at, allocation_count, resources, resets, fail_write, fail_read, fail_isids, fail_copy_user;
static int cached, credentials, stage_events;
static int rule_fail_at, rule_calls;
static void mutex_lock(pthread_mutex_t *lock)
{
    assert(!policy_locked);
    if (lock == &fsi.mutex) assert(hide_locked);
    assert(!pthread_mutex_lock(lock));
    if (lock == &selinux_hide_mutex) { assert(!hide_locked && !fs_locked); hide_locked = true; }
    if (lock == &fsi.mutex) { assert(!fs_locked); fs_locked = true; }
}
static void mutex_unlock(pthread_mutex_t *lock)
{
    if (lock == &fsi.mutex) { assert(fs_locked && hide_locked); fs_locked = false; }
    if (lock == &selinux_hide_mutex) { assert(hide_locked && !fs_locked && !policy_locked); hide_locked = false; }
    assert(!pthread_mutex_unlock(lock));
}
static void read_lock(int *lock) { assert(!policy_locked && hide_locked && fs_locked); policy_locked = true; }
static void read_unlock(int *lock) { assert(policy_locked); policy_locked = false; }
#define write_lock_irq(l) read_lock(l)
#define write_unlock_irq(l) read_unlock(l)
static void ksu_lock_sepolicy_legacy(void) { read_lock(&ss.policy_rwlock); }
static void ksu_unlock_sepolicy_legacy(void) { read_unlock(&ss.policy_rwlock); }
static bool selinux_initialized(struct selinux_state *s) { return s->initialized; }
static bool getenforce(void) { return true; }
static void *allocate(size_t n)
{
    assert(!policy_locked); allocation_count++;
    if (fault_at == allocation_count) return NULL;
    void *p = calloc(1,n); assert(p); resources++; return p;
}
static void release(void *p) { if (p) { assert(resources > 0); resources--; free(p); } }
static void *vmalloc(size_t n) { return allocate(n); }
static void vfree(void *p) { release(p); }
static void *kzalloc(size_t n, int flags) { return allocate(n); }
static void *kcalloc(size_t n, size_t sz, int flags) { return allocate(n*sz); }
static void kfree(void *p) { release(p); }
static void ksu_destroy_policydb(struct policydb *p)
{
    assert(hide_locked && p && p->resource);
    release(p->resource); p->resource = NULL;
}
static void sidtab_destroy(struct sidtab *s)
{
    assert(hide_locked && s && s->resource);
    release(s->resource); s->resource = NULL;
}
static int policydb_write(struct policydb *p, struct policy_file *fp)
{
    assert(policy_locked && p->resource && fp->len >= sizeof(p->marker));
    if (fail_write) return -EINVAL;
    memcpy(fp->data,&p->marker,sizeof(p->marker)); fp->len -= sizeof(p->marker); return 0;
}
static int policydb_read(struct policydb *p, struct policy_file *fp)
{
    assert(!policy_locked && !p->resource);
    if (fail_read) return -EINVAL; /* Failure contract: parser cleaned its own internals. */
    p->resource = allocate(8);
    if (!p->resource) return -ENOMEM;
    memcpy(&p->marker,fp->data,sizeof(p->marker)); p->p_types.nprim = 2;
    return 0;
}
static int policydb_load_isids(struct policydb *p, struct sidtab *s)
{
    assert(hide_locked && fs_locked && p->resource);
    s->resource = allocate(8);
    if (!s->resource) return -ENOMEM;
    if (fail_isids) { sidtab_destroy(s); return -EINVAL; }
    return 0;
}
static void reset_avc_cache(void) { assert(hide_locked && fs_locked && !policy_locked); resets++; }
static bool rule(struct policydb *p)
{ assert(hide_locked && fs_locked && !policy_locked && p->resource);
  rule_calls++; if (rule_calls == rule_fail_at) return false; p->marker++; return true; }
#define ksu_type(p,...) rule(p)
#define ksu_permissive(p,...) rule(p)
#define ksu_typeattribute(p,...) rule(p)
#define ksu_allow(p,...) rule(p)
#define ksu_allowxperm(p,...) rule(p)
static int copy_from_user(void *dest, const void *src, size_t len)
{ if (fail_copy_user) return 1; memcpy(dest,src,len); return 0; }
/* Minimal batch-parser stand-ins exercise actual handler ownership/branching. */
static int sepol_read_cmd_header(struct sepol_batch_cursor *c, struct sepol_data *h)
{ if (c->cur == c->end) return -EINVAL; h->cmd = *c->cur++; return 0; }
static int sepol_expected_argc(unsigned cmd) { return cmd == 2 ? -EINVAL : (cmd == 3 ? 1 : 0); }
static int sepol_read_string(struct sepol_batch_cursor *c, const char **s) { return -EINVAL; }
static int apply_one_sepolicy_cmd(struct policydb *p, const struct sepol_data *h, const char **args)
{ if (h->cmd == 4) return -EINVAL; p->marker++; return 0; }
#include "bakasu_policy_api.h"
#include "bakasu_policy_copy.h"
#include "bakasu_policy_lifecycle.h"

static bool check_argv(struct user_arg_ptr p, int index, const char *s, char *buf, size_t n)
{ return index < p.argc && p.args[index] && !strcmp(p.args[index],s); }
static int count(struct user_arg_ptr p, int max) { return p.argc; }
static const char *get_user_arg_ptr(struct user_arg_ptr p, int index) { return p.args[index]; }
static long ksu_strncpy_from_user_nofault(char *dest, const char *src, long n)
{ strncpy(dest,src,n); dest[n-1] = 0; return strlen(dest); }
static void ksu_selinux_hide_handle_second_stage(void) { stage_events++; }
static void cache_sid(void) { cached++; }
#ifdef KSU_COMPAT_DEVON_SELINUX_POLICY
static int cred_setup_failure;
static int setup_ksu_cred(void) { credentials++; return cred_setup_failure; }
#else
static void setup_ksu_cred(void) { credentials++; }
#endif
static void on_post_fs_data(void) {}
static void ksu_stop_ksud_execve_hook(void) {}
#include "bakasu_policy_boot.h"

static void cleanup(void)
{
    assert(!hide_locked && !fs_locked && !policy_locked);
    ksu_selinux_hide_running = false;
    ksu_selinux_hide_drop_backup_if_unused();
    release(ss.policydb.resource); ss.policydb.resource = NULL;
    assert(!resources);
}
static void fresh(void)
{
    assert(!resources); fault_at = allocation_count = resets = 0;
    fail_write = fail_read = fail_isids = fail_copy_user = 0;
    rule_fail_at = rule_calls = 0;
    selinux_state.ss = &ss; selinux_state.initialized = true; selinuxfs_mount = &mount;
    ss.policydb = (struct policydb){.len = 4, .p_types = {2}, .marker = 17, .resource = allocate(8)};
    ksu_devon_stock_captured = false; ksu_selinux_hide_running = false;
    allocation_count = 0;
}
static void unchanged(void)
{
    assert(ss.policydb.marker == 17 && !backup_policydb && !backup_sidtab);
    assert(resources == 1 && !ksu_devon_stock_captured && !resets);
    assert(!hide_locked && !fs_locked && !policy_locked);
}
static void test_init_faults(void)
{
    int covered = 0;
    for (int n=1; n<24; ++n) {
        fresh(); fault_at = n;
        int rc = apply_kernelsu_rules();
        if (!rc) { assert(covered == allocation_count); cleanup(); break; }
        assert(rc == -ENOMEM); unchanged(); covered++; cleanup();
    }
    assert(covered == 8);
    for (int i=0; i<3; ++i) {
        fresh(); if (!i) fail_write = 1; else if (i == 1) fail_read = 1; else fail_isids = 1;
        assert(apply_kernelsu_rules() == -EINVAL); unchanged(); cleanup();
    }
    fresh(); selinux_state.initialized = false;
    assert(apply_kernelsu_rules() == -EAGAIN); unchanged(); cleanup();
    fresh(); selinux_state.ss = NULL;
    assert(apply_kernelsu_rules() == -EAGAIN); selinux_state.ss = &ss; unchanged(); cleanup();
    fresh(); selinuxfs_mount = NULL;
    assert(apply_kernelsu_rules() == -EAGAIN); selinuxfs_mount = &mount; unchanged(); cleanup();
    const int critical[] = {1,2,6};
    for (unsigned i=0; i<sizeof(critical)/sizeof(critical[0]); i++) {
        fresh(); rule_fail_at = critical[i];
        assert(apply_kernelsu_rules() == -EINVAL); unchanged(); cleanup();
    }
    puts("PASS: eight private-copy allocation failures, serializer/parser/ISID and critical-rule failures, policy readiness unwind");
}
static void test_snapshot(void)
{
    fresh(); assert(!apply_kernelsu_rules());
    struct policydb *original = backup_policydb; struct sidtab *table = backup_sidtab;
    assert(original->marker == 17 && ss.policydb.marker > 17 && resets == 1);
    ksu_selinux_hide_running = true;
    assert(!apply_kernelsu_rules() && backup_policydb == original && backup_sidtab == table);
    assert(original->marker == 17 && resets == 2);
    ksu_selinux_hide_drop_backup_if_unused(); assert(backup_policydb == original);
    ksu_selinux_hide_running = false; ksu_selinux_hide_drop_backup_if_unused();
    assert(!backup_policydb && !backup_sidtab && ksu_devon_stock_captured);
    assert(!apply_kernelsu_rules() && !backup_policydb && !backup_sidtab);
    cleanup();
    puts("PASS: first snapshot survives repeated installation and is never recreated after retirement");
}
static void test_batch(void)
{
    const u8 valid[] = {1,1}, bad[] = {1,2}, string_bad[] = {1,3}, mixed[] = {1,4};
    fresh(); assert(handle_sepolicy((void *)valid,sizeof(valid)) == -EAGAIN); unchanged(); cleanup();
    fresh(); assert(!apply_kernelsu_rules());
    int marker = ss.policydb.marker, objects = resources, notified = resets;
    assert(handle_sepolicy(NULL,2) == -EINVAL);
    assert(handle_sepolicy((void *)valid,0) == -EINVAL);
    assert(handle_sepolicy((void *)valid,KSU_SEPOLICY_MAX_BATCH_SIZE+1) == -E2BIG);
    fail_copy_user = 1; assert(handle_sepolicy((void *)valid,2) == -EFAULT); fail_copy_user = 0;
    assert(resources == objects && ss.policydb.marker == marker && resets == notified);
    for (int n=1; n<=4; ++n) {
        allocation_count = 0; fault_at = n;
        assert(handle_sepolicy((void *)valid,2) == -ENOMEM);
        assert(resources == objects && ss.policydb.marker == marker && resets == notified);
        assert(!hide_locked && !fs_locked && !policy_locked);
    }
    fault_at = 0;
    assert(handle_sepolicy((void *)bad,2) == -EINVAL);
    assert(handle_sepolicy((void *)string_bad,2) == -EINVAL);
    assert(resources == objects && ss.policydb.marker == marker && resets == notified);
    assert(handle_sepolicy((void *)valid,2) == 2 && ss.policydb.marker == marker+2);
    assert(backup_policydb->marker == 17 && resources == objects);
    assert(handle_sepolicy((void *)mixed,2) == 1 && ss.policydb.marker == marker+3);
    cleanup();
    puts("PASS: pre-lock user errors, batch-copy allocation faults, malformed-batch rollback and existing partial-success count");
}
static void test_capacity(void)
{
    fresh(); struct policydb dest = {0};
    assert(ksu_dup_policydb(NULL,&dest) == -EINVAL);
    ksu_selinux_hide_lock(); mutex_lock(&fsi.mutex);
    ss.policydb.len = INT_MAX;
    assert(ksu_dup_policydb(&ss.policydb,&dest) == -EOVERFLOW);
    ss.policydb.len = SIZE_MAX;
    assert(ksu_dup_policydb(&ss.policydb,&dest) == -EOVERFLOW);
    ss.policydb.len = 0; ss.policydb.p_types.nprim = 0;
    assert(ksu_dup_policydb(&ss.policydb,&dest) == -EOVERFLOW);
    mutex_unlock(&fsi.mutex); ksu_selinux_hide_unlock(); cleanup();
    puts("PASS: null copy arguments, signed-length limit, size overflow and empty capacity");
}
static void test_boot(void)
{
    fresh(); struct user_arg_ptr argv = {.argc = 2, .args = {"init","second_stage"}};
    struct user_arg_ptr env = {.argc = 1, .args = {NULL,"INIT_SECOND_STAGE=1"}};
    char name[64] = "/system/bin/init";
#if INIT_PATH == 1
    strcpy(name,"/init"); argv.args[1] = "--second-stage";
#elif INIT_PATH == 2
    strcpy(name,"/init"); argv.argc = 1;
#endif
    fault_at = 1;
    ksu_handle_execveat_ksud(name,&argv,&env,NULL);
    assert(!cached && !credentials && stage_events == 1); unchanged();
    fault_at = 0;
#ifdef KSU_COMPAT_DEVON_SELINUX_POLICY
    cred_setup_failure = -EINVAL;
    ksu_handle_execveat_ksud(name,&argv,&env,NULL);
    assert(cached == 1 && credentials == 1 && stage_events == 2);
    cred_setup_failure = 0;
    cached = credentials = 0; stage_events = 1;
#endif
    ksu_handle_execveat_ksud(name,&argv,&env,NULL);
    assert(cached == 1 && credentials == 1 && stage_events == 2 && ksu_devon_stock_captured);
    ksu_handle_execveat_ksud(name,&argv,&env,NULL);
    assert(cached == 1 && credentials == 1 && stage_events == 2);
    cleanup();
    puts("PASS: failed second-stage policy setup skips SID/credential caching, retries successfully and completes once");
}
int main(void)
{
    test_init_faults(); test_snapshot(); test_batch(); test_capacity(); test_boot();
    return 0;
}
