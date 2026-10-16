/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_BAKASU_MANUAL_H
#define _LINUX_BAKASU_MANUAL_H

#include <linux/types.h>

#ifdef CONFIG_KSU_MANUAL_HOOK
struct filename;
struct stat;
struct stat64;

int ksu_handle_execveat(int *fd, struct filename **filename, void *argv,
			void *envp, int *flags);
int ksu_handle_post_execveat(int *fd, struct filename **filename, void *argv,
			     void *envp, int *flags, int *retval);
int ksu_handle_faccessat(int *dfd, const char __user **filename, int *mode,
			int *flags);
int ksu_handle_stat(int *dfd, const char __user **filename, int *flags);
void ksu_handle_newfstat_ret(unsigned int *fd, struct stat __user **statbuf);
#if defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64)
void ksu_handle_fstat64_ret(unsigned long *fd, struct stat64 __user **statbuf);
#endif
int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int cmd,
			  void __user **arg);
#endif

#endif
