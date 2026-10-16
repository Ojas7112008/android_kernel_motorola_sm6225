/* SPDX-License-Identifier: GPL-2.0 */
/* Real syscall/provider bodies; all reboot, namespace and driver actions mocked. */
#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#define __user
#define pr_info(...) ((void)0)
#define pr_err(...) ((void)0)
#define CAP_SYS_BOOT 22
#define CONFIG_KEXEC_CORE 1
#define CONFIG_HIBERNATION 1
#define CONFIG_KSU_TOOLKIT_SUPPORT 1
#define likely(v) (v)
struct task_struct { int pid; } task = {123}, *current = &task;
struct kuid { uid_t val; };
static uid_t real_uid;
static struct kuid current_uid(void) { return (struct kuid){real_uid}; }
static uid_t ksu_get_uid_t(struct kuid uid) { return uid.val; }
static int test_thread_flag(int bit) { return 0; }
static void set_thread_flag(int bit) {}
static void clear_thread_flag(int bit) {}
static void clear_tsk_thread_flag(struct task_struct *t, int bit) {}
#include "reboot_magic.h"
#include "native_reboot_constants.h"
#ifdef CONFIG_KSU_SUSFS
#include <linux/ksu_susfs.h>
#endif
struct pid_namespace { int user_ns; } ns;
static bool capable, copy_fault, string_fault;
static int capability_calls, namespace_calls, ns_result, install_result;
static int install_calls, close_calls, closed_fd, toolkit_calls, susfs_calls;
static int extension_result, extension_error, native_calls, transition_locked;
static unsigned int received_cmd;
static void *received_arg;
static int C_A_D = 1, system_transition_mutex;
static void (*pm_power_off)(void);
static jmp_buf exit_target;
static struct pid_namespace *task_active_pid_ns(struct task_struct *t) { return &ns; }
static bool ns_capable(int user_ns, int cap) {
    assert(cap == CAP_SYS_BOOT && user_ns == ns.user_ns);
    capability_calls++; return capable;
}
static int reboot_pid_ns(struct pid_namespace *n, unsigned int cmd) {
    assert(n == &ns); namespace_calls++; return ns_result;
}
static int ksu_install_fd(void) { install_calls++; return install_result; }
static int copy_to_user(void *dst, const void *src, size_t size) {
    if (copy_fault || !dst) return 1;
    memcpy(dst, src, size); return 0;
}
static void ksu_close_fd(int fd) { close_calls++; closed_fd = fd; }
static int ksu_try_handle_toolkit_cmd(int magic2, unsigned int cmd, void **arg) {
    toolkit_calls++; assert(*arg == received_arg);
    return (unsigned int)magic2 == 0xABCD1234U;
}
static int ksu_handle_susfs_cmd(unsigned int cmd, void **arg) {
    assert(arg && *arg == received_arg);
    susfs_calls++; received_cmd = cmd;
    if (!copy_fault && *arg) *(int *)*arg = extension_error;
    return extension_result;
}
#if defined(CONFIG_KSU_MANUAL_HOOK) || defined(CONFIG_KSU_SUSFS)
#include "provider_reboot.h"
#endif
static void mutex_lock(int *lock) { assert(!transition_locked); transition_locked = 1; }
static void mutex_unlock(int *lock) { assert(transition_locked); transition_locked = 0; }
static void kernel_restart(const char *cmd) {
    assert(transition_locked); native_calls++;
    if (cmd) assert(!strcmp(cmd, "bootloader"));
}
static void kernel_halt(void) { assert(transition_locked); native_calls++; }
static void kernel_power_off(void) { assert(transition_locked); native_calls++; }
static void do_exit(int code) { assert(!code); longjmp(exit_target, 1); }
static void panic(const char *message) { assert(!"unreachable panic"); }
static int kernel_kexec(void) { native_calls++; return -EBUSY; }
static int hibernate(void) { native_calls++; return -EIO; }
static long strncpy_from_user(char *dst, const char *src, size_t max) {
    if (string_fault || !src) return -EFAULT;
    size_t n = strnlen(src, max); memcpy(dst, src, n);
    if (n < max) dst[n] = 0;
    return n;
}
#define SYSCALL_DEFINE4(name,t1,a1,t2,a2,t3,a3,t4,a4) \
    static long kernel_##name(t1 a1,t2 a2,t3 a3,t4 a4)
#include "kernel_reboot.h"
static void reset(void) {
    capable = true; copy_fault = string_fault = false;
    real_uid = 0; ns.user_ns = 7; ns_result = 0;
    capability_calls = namespace_calls = install_calls = close_calls = toolkit_calls = susfs_calls = 0;
    native_calls = transition_locked = 0; install_result = 19;
    extension_result = -EFAULT; extension_error = -EOPNOTSUPP;
    pm_power_off = NULL;
}
static void transport_tests(void) {
    int reply = -1;
    reset(); capable = false; real_uid = 10001; received_arg = &reply;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, &reply) == -EPERM);
#if defined(CONFIG_KSU_MANUAL_HOOK) || defined(CONFIG_KSU_SUSFS)
    assert(reply == 19 && install_calls == 1 && !close_calls);
#else
    assert(reply == -1 && !install_calls);
#endif
    assert(capability_calls == 1 && !namespace_calls && !native_calls);
    reset(); reply = -1; received_arg = &reply;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, &reply) == -EINVAL);
    assert(!namespace_calls && !native_calls);
    reset(); install_result = -ENOMEM; received_arg = &reply;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, &reply) == -EINVAL);
#if defined(CONFIG_KSU_MANUAL_HOOK) || defined(CONFIG_KSU_SUSFS)
    assert(reply == -ENOMEM);
#endif
    reset(); copy_fault = true; received_arg = &reply;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, &reply) == -EINVAL);
#if defined(CONFIG_KSU_MANUAL_HOOK) || defined(CONFIG_KSU_SUSFS)
    assert(close_calls == 1 && closed_fd == 19);
#endif
#ifdef CONFIG_KSU_SUSFS
    reset(); capable = false; reply = 987; received_arg = &reply;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, SUSFS_MAGIC, CMD_SUSFS_SHOW_VERSION, &reply) == -EPERM);
    assert(susfs_calls == 1 && received_cmd == CMD_SUSFS_SHOW_VERSION && reply == extension_error);
    assert(!namespace_calls && !native_calls && !install_calls);
    reset(); reply = 987; real_uid = 10001; received_arg = &reply;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, SUSFS_MAGIC, CMD_SUSFS_SHOW_VERSION, &reply) == -EINVAL);
    assert(!susfs_calls && !toolkit_calls && reply == 987);
    reset(); received_arg = NULL; copy_fault = true;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, SUSFS_MAGIC, 0xFFFFFFFFU, NULL) == -EINVAL);
    assert(susfs_calls == 1 && !native_calls && !namespace_calls);
#endif
    reset(); received_arg = &reply;
    assert(kernel_reboot(KSU_INSTALL_MAGIC1, 0xABCD1234U, 999, &reply) == -EINVAL);
#if defined(CONFIG_KSU_MANUAL_HOOK) || defined(CONFIG_KSU_SUSFS)
    assert(toolkit_calls == 1 && !susfs_calls);
#endif
    reset(); received_arg = &reply;
    assert(kernel_reboot(123, KSU_INSTALL_MAGIC2, 0, &reply) == -EINVAL);
    assert(!install_calls && !toolkit_calls && !susfs_calls && !native_calls);
}
static void native_tests(void) {
    unsigned int magics[] = {LINUX_REBOOT_MAGIC2, LINUX_REBOOT_MAGIC2A, LINUX_REBOOT_MAGIC2B, LINUX_REBOOT_MAGIC2C};
    for (size_t i = 0; i < sizeof(magics)/sizeof(*magics); i++) {
        reset(); assert(!kernel_reboot(LINUX_REBOOT_MAGIC1, magics[i], LINUX_REBOOT_CMD_CAD_OFF, NULL));
        assert(!C_A_D && namespace_calls == 1 && !native_calls && !transition_locked);
        assert(!kernel_reboot(LINUX_REBOOT_MAGIC1, magics[i], LINUX_REBOOT_CMD_CAD_ON, NULL)); assert(C_A_D);
        assert(!install_calls && !toolkit_calls && !susfs_calls);
    }
    reset(); capable = false;
    assert(kernel_reboot(123, 456, LINUX_REBOOT_CMD_RESTART, NULL) == -EPERM && !native_calls);
    reset(); ns_result = -ESRCH;
    assert(kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART, NULL) == -ESRCH);
    assert(namespace_calls == 1 && !native_calls && !transition_locked);
    reset(); assert(!kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART, NULL));
    assert(native_calls == 1 && !transition_locked);
    reset(); assert(kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "bootloader") == 10);
    assert(native_calls == 1 && !transition_locked);
    reset(); string_fault = true;
    assert(kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, NULL) == -EFAULT);
    assert(!native_calls && !transition_locked);
    reset(); assert(kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_KEXEC, NULL) == -EBUSY);
    reset(); assert(kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_SW_SUSPEND, NULL) == -EIO);
    reset(); assert(kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, 0xFFFFFFFFU, NULL) == -EINVAL);
    assert(!native_calls && !transition_locked);
    reset();
    if (!setjmp(exit_target)) { kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL); assert(!"halt must exit"); }
    assert(native_calls == 1 && transition_locked); transition_locked = 0;
    reset(); pm_power_off = (void (*)(void))1;
    if (!setjmp(exit_target)) { kernel_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL); assert(!"power off must exit"); }
    assert(native_calls == 1 && transition_locked); transition_locked = 0;
}
int main(void) {
    transport_tests(); native_tests();
    puts("PASS: actual reboot/provider bodies preserve native checks, namespace/errors and command behavior; transport replies/auth/routing use mocks only");
    return 0;
}
