/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KSU_SUSFS_H
#define _LINUX_KSU_SUSFS_H

#include <linux/types.h>

#ifdef CONFIG_KSU_SUSFS
#if !defined(CONFIG_KSU) || !defined(CONFIG_KSU_DEVON_SELINUX_POLICY) || !defined(CONFIG_ARM64)
#error "devon SUSFS hooks require built-in ARM64 KSU and its checked policy path"
#endif
#include <linux/susfs_def.h>

/* BakaSU's pending exec marker; mount/restriction flags survive fork and exec. */
#define KSU_SUSFS_TIF_IN_EXECVE 61

struct filename;
struct file;
struct iov_iter;

/* Successful lookup consumes *filename; on hook error the caller releases it. */
int ksu_handle_faccessat(int *dfd, struct filename **filename, int *mode,
			int *flags);
int ksu_handle_stat(int *dfd, struct filename **filename, int *flags);
int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid);
bool ksu_susfs_cred_ready(void);

/* Reboot transport replies through the user buffer, before native checks. */
int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int cmd,
			  void __user **arg);
/* The caller supplies copies of an accepted key press under event_lock. */
int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code,
				int *value);

/* Each caller keeps the native file alive through the hook and any read. */
int ksu_handle_vfs_fstat_file(struct file *file, loff_t *size);
int ksu_handle_sys_read_file(struct file *file, char __user *buf, size_t *count,
			    loff_t *pos, ssize_t *result);
int ksu_handle_initrc_iter_file(struct file *file, struct iov_iter *iter,
			      loff_t *pos, size_t *limit, bool synchronous,
			      ssize_t *result);
ssize_t ksu_handle_initrc_read_result(struct file *file, loff_t before,
				     ssize_t result);
int ksu_handle_initrc_seek_file(struct file *file, loff_t offset, int whence,
			      loff_t *result);
void ksu_handle_initrc_file_release(struct file *file);

static inline void ksu_susfs_reset_fork_state(struct task_struct *task)
{
	clear_tsk_thread_flag(task, KSU_SUSFS_TIF_IN_EXECVE);
}
#endif

#endif /* _LINUX_KSU_SUSFS_H */
