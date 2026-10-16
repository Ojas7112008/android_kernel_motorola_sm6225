/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __user
#define __NEW_UTS_LEN 64
#define GFP_KERNEL 0
#define DEFINE_MUTEX(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define mutex_lock(p) assert(!pthread_mutex_lock(p))
#define mutex_unlock(p) assert(!pthread_mutex_unlock(p))
struct new_utsname { char sysname[65], nodename[65], release[65], version[65], machine[65], domainname[65]; };
static struct new_utsname model_uts = { .release = "native-release", .version = "native-version", .machine = "aarch64" };
static struct new_utsname *utsname(void) { return &model_uts; }
static pthread_rwlock_t uts_sem = PTHREAD_RWLOCK_INITIALIZER;
#define down_read(p) assert(!pthread_rwlock_rdlock(p))
#define up_read(p) assert(!pthread_rwlock_unlock(p))
static bool copy_fault, alloc_fault;
static size_t copy_from_user(void *dst, const void *src, size_t n)
{ if (copy_fault || !src) return n; memcpy(dst, src, n); return 0; }
static size_t copy_to_user(void *dst, const void *src, size_t n)
{ if (!dst) return n; memcpy(dst, src, n); return 0; }
static void *kmalloc(size_t n, int flags) { return alloc_fault ? NULL : malloc(n); }
static void *kzalloc(size_t n, int flags) { return alloc_fault ? NULL : calloc(1, n); }
#define kfree(p) free(p)
static ssize_t strscpy(char *dst, const char *src, size_t n)
{ size_t len = strlen(src); if (!n) return -E2BIG; snprintf(dst, n, "%s", src); return len < n ? (ssize_t)len : -E2BIG; }
struct seq_file { char buf[20000]; size_t len; void *private; };
static void seq_puts(struct seq_file *m, const char *s)
{ size_t n = strlen(s); assert(m->len + n < sizeof(m->buf)); memcpy(m->buf + m->len, s, n + 1); m->len += n; }
static void seq_putc(struct seq_file *m, char c)
{ char s[2] = {c, 0}; seq_puts(m, s); }
static void seq_printf(struct seq_file *m, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); int n = vsnprintf(m->buf + m->len, sizeof(m->buf) - m->len, fmt, ap); va_end(ap); assert(n >= 0 && m->len + n < sizeof(m->buf)); m->len += n; }
static char *saved_command_line = "native=cmdline";
static int override_release(char *p, size_t n) { return 0; }
#define override_architecture(name) 0
#define SYSCALL_DEFINE1(name, type, arg) static long native_##name(type arg)
#define SUSFS_LOGI(...) do {} while (0)
static void susfs_reply_error(void **p, size_t offset, int err)
{ if (p && *p) memcpy((char *)*p + offset, &err, sizeof(err)); }
struct kallsym_iter { bool show_value, exported; unsigned long value; char type; char name[128], module_name[128]; };

#include "linux/susfs.h"
#include "linux/susfs_spoof.h"
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
#include "uname_handler.h"
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
#include "cmdline_handler.h"
#endif
#include "native_uname.h"
#include "native_cmdline.h"
#include "native_symbols.h"
#include "features_query.h"

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
static void set_uname_pair(const char *release, const char *version)
{
	struct st_susfs_uname info = { .err = 99 };
	void *p = &info;
	strscpy(info.release, release, sizeof(info.release));
	strscpy(info.version, version, sizeof(info.version));
	susfs_set_uname(&p);
	assert(info.err == 0);
}
static void test_uname(void)
{
	struct new_utsname output;
	struct st_susfs_uname bad = { .release = "valid", .version = "valid", .err = 99 };
	void *p = &bad;
	assert(native_newuname(&output) == 0 && !strcmp(output.release, "native-release"));
	set_uname_pair("custom-release", "custom-version");
	assert(!native_newuname(&output));
	assert(!strcmp(output.release, "custom-release") && !strcmp(output.version, "custom-version"));
	assert(!strcmp(output.machine, "aarch64") && !strcmp(model_uts.release, "native-release"));
	memset(bad.release, 'x', sizeof(bad.release));
	susfs_set_uname(&p); assert(bad.err == -EINVAL);
	bad.release[0] = 0; susfs_set_uname(&p); assert(bad.err == -EINVAL);
	strscpy(bad.release, "valid", sizeof(bad.release));
	memset(bad.version, 'x', sizeof(bad.version)); susfs_set_uname(&p); assert(bad.err == -EINVAL);
	copy_fault = true; susfs_set_uname(&p); assert(bad.err == -EFAULT); copy_fault = false;
	assert(!native_newuname(&output) && !strcmp(output.release, "custom-release"));
	set_uname_pair("default", "default");
	assert(!native_newuname(&output) && !strcmp(output.release, "native-release"));
	char max[65]; memset(max, 'm', 64); max[64] = 0; set_uname_pair(max, max);
	assert(!native_newuname(&output) && strlen(output.release) == 64 && strlen(output.version) == 64);
	p = NULL; susfs_set_uname(&p); susfs_set_uname(NULL);
	assert(native_newuname(NULL) == -EFAULT);
}
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
static void set_cmdline(const char *text)
{
	struct st_susfs_spoof_cmdline_or_bootconfig *info = calloc(1, sizeof(*info));
	void *p = info;
	strscpy(info->fake_cmdline_or_bootconfig, text, sizeof(info->fake_cmdline_or_bootconfig));
	susfs_set_cmdline_or_bootconfig(&p); assert(!info->err); free(info);
}
static void test_cmdline(void)
{
	struct seq_file m = {0};
	struct st_susfs_spoof_cmdline_or_bootconfig *info = calloc(1, sizeof(*info));
	void *p = info;
	assert(!cmdline_proc_show(&m, NULL) && !strcmp(m.buf, "native=cmdline\n"));
	set_cmdline("fake=1"); m = (struct seq_file){0};
	assert(!cmdline_proc_show(&m, NULL) && !strcmp(m.buf, "fake=1\n"));
	set_cmdline("fake=2\n"); m = (struct seq_file){0};
	assert(!cmdline_proc_show(&m, NULL) && !strcmp(m.buf, "fake=2\n"));
	susfs_set_cmdline_or_bootconfig(&p); assert(info->err == -EINVAL);
	memset(info->fake_cmdline_or_bootconfig, 'x', sizeof(info->fake_cmdline_or_bootconfig));
	susfs_set_cmdline_or_bootconfig(&p); assert(info->err == -EINVAL);
	info->fake_cmdline_or_bootconfig[8191] = 0;
	susfs_set_cmdline_or_bootconfig(&p); assert(!info->err); m = (struct seq_file){0};
	assert(!cmdline_proc_show(&m, NULL) && m.len == 8192);
	alloc_fault = true; susfs_set_cmdline_or_bootconfig(&p); assert(info->err == -ENOMEM); alloc_fault = false;
	copy_fault = true; susfs_set_cmdline_or_bootconfig(&p); assert(info->err == -EFAULT); copy_fault = false;
	m = (struct seq_file){0}; assert(!cmdline_proc_show(&m, NULL) && m.len == 8192);
	p = NULL; susfs_set_cmdline_or_bootconfig(&p); susfs_set_cmdline_or_bootconfig(NULL);
	free(info);
}
#endif

static void test_symbols(void)
{
	struct seq_file m = {0};
	struct kallsym_iter iter = { .type = 't', .name = "ksu_handle_susfs_cmd" };
	m.private = &iter; assert(!s_show(&m, NULL));
#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
	assert(!m.len);
	const char *hidden[] = {"ksu_test", "__ksu_test", "susfs_test", "__cfi_ksu_test", "__pfx_susfs_test", "kernelsu_init", "is_zygote_test"};
	for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); i++) assert(susfs_hide_symbol(hidden[i]));
	assert(!susfs_hide_symbol("unrelated_ksu_test") && !susfs_hide_symbol("do_sys_open") && !susfs_hide_symbol("__cfi_check"));
#else
	assert(strstr(m.buf, iter.name));
#endif
	m.len = 0; m.buf[0] = 0; strcpy(iter.module_name, "unrelated_module");
	assert(!s_show(&m, NULL) && strstr(m.buf, iter.name));
	m.len = 0; m.buf[0] = 0; iter.module_name[0] = 0; strcpy(iter.name, "do_sys_open");
	assert(!s_show(&m, NULL) && strstr(m.buf, iter.name));
}

#if defined(CONFIG_KSU_SUSFS_SPOOF_UNAME) || defined(CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG)
static void *writer(void *arg)
{
	for (int i = 0; i < 1500; i++) {
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
		set_uname_pair(i % 2 ? "aaaa" : "bbbb", i % 2 ? "AAAA" : "BBBB");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
		set_cmdline(i % 2 ? "aaaaaaaa" : "bbbbbbbb");
#endif
	}
	return NULL;
}
static void *reader(void *arg)
{
	for (int i = 0; i < 1500; i++) {
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
		struct new_utsname u;
		assert(!native_newuname(&u));
		assert((!strcmp(u.release, "aaaa") && !strcmp(u.version, "AAAA")) ||
		       (!strcmp(u.release, "bbbb") && !strcmp(u.version, "BBBB")));
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
		struct seq_file m = {0}; assert(!cmdline_proc_show(&m, NULL));
		assert(!strcmp(m.buf, "aaaaaaaa\n") || !strcmp(m.buf, "bbbbbbbb\n"));
#endif
	}
	return NULL;
}
static void concurrency(void)
{
	pthread_t a, b, c;
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	set_uname_pair("aaaa", "AAAA");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	set_cmdline("aaaaaaaa");
#endif
	assert(!pthread_create(&a, NULL, writer, NULL));
	assert(!pthread_create(&b, NULL, writer, NULL));
	assert(!pthread_create(&c, NULL, reader, NULL));
	assert(!pthread_join(a, NULL) && !pthread_join(b, NULL) && !pthread_join(c, NULL));
}
#endif

static void capabilities(void)
{
	struct st_susfs_enabled_features *info = malloc(sizeof(*info));
	void *p = info;
	memset(info, 'X', sizeof(*info)); susfs_get_enabled_features(&p);
	assert(info->err == 0 && !strchr(info->enabled_features, 'X'));
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	assert(strstr(info->enabled_features, "CONFIG_KSU_SUSFS_SPOOF_UNAME\n"));
#else
	assert(!strstr(info->enabled_features, "CONFIG_KSU_SUSFS_SPOOF_UNAME\n"));
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	assert(strstr(info->enabled_features, "CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG\n"));
#else
	assert(!strstr(info->enabled_features, "CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG\n"));
#endif
#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
	assert(strstr(info->enabled_features, "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS\n"));
#else
	assert(!strstr(info->enabled_features, "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS\n"));
#endif
#include "capabilities_expected.h"
	free(info);
}
int main(void)
{
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	test_uname();
#else
	struct new_utsname u; assert(!native_newuname(&u) && !strcmp(u.release, "native-release"));
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	test_cmdline();
#else
	struct seq_file m = {0}; assert(!cmdline_proc_show(&m, NULL) && !strcmp(m.buf, "native=cmdline\n"));
#endif
	test_symbols(); capabilities();
#if defined(CONFIG_KSU_SUSFS_SPOOF_UNAME) || defined(CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG)
	concurrency();
#endif
	puts("identity handlers, native consumers, bounds/faults, capabilities and modeled concurrent snapshots passed");
}
