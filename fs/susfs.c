// SPDX-License-Identifier: GPL-2.0
/* Experimental SUSFS 2.3.0 devon port; see docs/SUSFS_2_3_ALL_FEATURES.md. */
#include <linux/atomic.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fsnotify_backend.h>
#include <linux/jiffies.h>
#include <linux/jump_label.h>
#include <linux/kernel.h>
#include <linux/ksu_susfs.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/overflow.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/susfs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#if !defined(CONFIG_KSU_SUSFS) || !defined(CONFIG_FSNOTIFY)
#error "devon SUSFS core requires its checked built-in KSU route and FSNOTIFY"
#endif

extern struct cred *ksu_cred;

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
DEFINE_STATIC_KEY_TRUE(susfs_is_log_enabled);
#define SUSFS_LOGI(fmt, ...) do { \
	if (static_branch_likely(&susfs_is_log_enabled)) \
		pr_info("susfs: " fmt, ##__VA_ARGS__); \
} while (0)
#else
#define SUSFS_LOGI(fmt, ...) do { } while (0)
#endif

static void susfs_reply_error(void __user **user_info, size_t offset, int err)
{
	unsigned long address;

	if (!user_info || !*user_info ||
	    check_add_overflow((unsigned long)*user_info, (unsigned long)offset, &address) ||
	    copy_to_user((void __user *)address, &err, sizeof(err)))
		SUSFS_LOGI("failed to return command error: %d\n", err);
}

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_enable_log(void __user **user_info)
{
	/* Read bool as bytes: reject invalid user representations before loading. */
	unsigned char request[sizeof(struct st_susfs_log)];
	int err = -EFAULT;

	if (!user_info || copy_from_user(request, *user_info, sizeof(request)))
		goto reply;
	if (request[offsetof(struct st_susfs_log, enabled)] > 1) {
		err = -EINVAL;
		goto reply;
	}
	if (request[offsetof(struct st_susfs_log, enabled)])
		static_branch_enable(&susfs_is_log_enabled);
	else
		static_branch_disable(&susfs_is_log_enabled);
	err = 0;
reply:
	susfs_reply_error(user_info, offsetof(struct st_susfs_log, err), err);
}
#endif

void susfs_set_avc_log_spoofing(void __user **user_info)
{
	/* No AVC consumer has been ported, so never report an effective toggle. */
	susfs_reply_error(user_info, offsetof(struct st_susfs_avc_log_spoofing, err),
			  -EOPNOTSUPP);
}

void susfs_get_enabled_features(void __user **user_info)
{
	struct st_susfs_enabled_features *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		susfs_reply_error(user_info,
				offsetof(struct st_susfs_enabled_features, err), -ENOMEM);
		return;
	}
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
	strscpy(info->enabled_features, "CONFIG_KSU_SUSFS_ENABLE_LOG\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	strlcat(info->enabled_features, "CONFIG_KSU_SUSFS_SPOOF_UNAME\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	strlcat(info->enabled_features,
		"CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
	strlcat(info->enabled_features, "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	strlcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_MAP\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	strlcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_KSTAT\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	strlcat(info->enabled_features, "CONFIG_KSU_SUSFS_OPEN_REDIRECT\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	strlcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_PATH\n",
		sizeof(info->enabled_features));
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	strlcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_MOUNT\n",
		sizeof(info->enabled_features));
#endif
	/* Output-only query: zero padding/tail and no echoed user feature names. */
	if (!user_info || copy_to_user(*user_info, info, sizeof(*info)))
		SUSFS_LOGI("failed to return enabled features\n");
	kfree(info);
}

void susfs_show_variant(void __user **user_info)
{
	struct st_susfs_variant info = { 0 };

	strscpy(info.susfs_variant, SUSFS_VARIANT, sizeof(info.susfs_variant));
	if (!user_info || copy_to_user(*user_info, &info, sizeof(info)))
		SUSFS_LOGI("failed to return variant\n");
}

void susfs_show_version(void __user **user_info)
{
	struct st_susfs_version info = { 0 };

	strscpy(info.susfs_version, SUSFS_VERSION, sizeof(info.susfs_version));
	if (!user_info || copy_to_user(*user_info, &info, sizeof(info)))
		SUSFS_LOGI("failed to return version\n");
}

#define SDCARD_ANDROID_PATH "/data/media/0/Android"
#define SDCARD_RETRY_DELAY (5 * HZ)
DEFINE_STATIC_KEY_TRUE(susfs_is_sdcard_android_data_not_decrypted);

/* Worker owns these references under the mutex; callbacks only request work. */
static DEFINE_MUTEX(susfs_sdcard_lock);
static struct fsnotify_group *susfs_sdcard_group;
static struct fsnotify_mark *susfs_sdcard_mark;
static struct path susfs_sdcard_path;
static atomic_t susfs_sdcard_started = ATOMIC_INIT(0);
static atomic_t susfs_sdcard_done = ATOMIC_INIT(0);
static atomic_t susfs_sdcard_watch_lost = ATOMIC_INIT(0);
static void susfs_sdcard_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(susfs_sdcard_work, susfs_sdcard_worker);

static void susfs_sdcard_wake(void)
{
	if (atomic_read(&susfs_sdcard_started) && !atomic_read(&susfs_sdcard_done))
		mod_delayed_work(system_unbound_wq, &susfs_sdcard_work, 0);
}

/* Called inside fsnotify_mark_srcu: no lookup, locks or group destruction. */
static int susfs_handle_sdcard_event(struct fsnotify_group *group,
				   struct inode *inode, u32 mask,
				   const void *data, int data_type,
				   const unsigned char *file_name, u32 cookie,
				   struct fsnotify_iter_info *iter_info)
{
	if (mask & (FS_DELETE_SELF | FS_MOVE_SELF)) {
		atomic_set(&susfs_sdcard_watch_lost, 1);
		susfs_sdcard_wake();
	} else if ((mask & (FS_OPEN | FS_CREATE | FS_MOVED_TO)) &&
		   (mask & FS_ISDIR) && file_name &&
		   !strcmp((const char *)file_name, "Android")) {
		susfs_sdcard_wake();
	}
	/* This observer never denies an operation. */
	return 0;
}

static void susfs_sdcard_freeing_mark(struct fsnotify_mark *mark,
				     struct fsnotify_group *group)
{
	atomic_set(&susfs_sdcard_watch_lost, 1);
	susfs_sdcard_wake();
}

static void susfs_sdcard_free_mark(struct fsnotify_mark *mark)
{
	kfree(mark);
}

static const struct fsnotify_ops susfs_sdcard_ops = {
	.handle_event = susfs_handle_sdcard_event,
	.freeing_mark = susfs_sdcard_freeing_mark,
	.free_mark = susfs_sdcard_free_mark,
};

static void susfs_sdcard_release_watch(void)
{
	/* Drop the owner's mark before destroy_group waits for final mark frees. */
	if (susfs_sdcard_mark) {
		fsnotify_put_mark(susfs_sdcard_mark);
		susfs_sdcard_mark = NULL;
	}
	if (susfs_sdcard_group) {
		fsnotify_destroy_group(susfs_sdcard_group);
		susfs_sdcard_group = NULL;
	}
	if (susfs_sdcard_path.mnt) {
		path_put(&susfs_sdcard_path);
		memset(&susfs_sdcard_path, 0, sizeof(susfs_sdcard_path));
	}
}

static int susfs_sdcard_watch_parent(void)
{
	struct path path;
	struct inode *inode;
	struct fsnotify_group *group;
	struct fsnotify_mark *mark;
	int ret;

	ret = kern_path("/data/media/0", LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &path);
	if (ret)
		return ret;
	inode = d_backing_inode(path.dentry);
	if (!inode || !S_ISDIR(inode->i_mode)) {
		ret = -ENOTDIR;
		goto put_path;
	}
	group = fsnotify_alloc_group(&susfs_sdcard_ops);
	if (IS_ERR(group)) {
		ret = PTR_ERR(group);
		goto put_path;
	}
	mark = kzalloc(sizeof(*mark), GFP_KERNEL);
	if (!mark) {
		ret = -ENOMEM;
		goto destroy_group;
	}
	fsnotify_init_mark(mark, group);
	mark->mask = FS_EVENT_ON_CHILD | FS_OPEN | FS_CREATE | FS_MOVED_TO |
		     FS_DELETE_SELF | FS_MOVE_SELF;
	ret = fsnotify_add_inode_mark(mark, inode, 0);
	if (ret) {
		fsnotify_put_mark(mark);
		goto destroy_group;
	}
	susfs_sdcard_path = path;
	susfs_sdcard_group = group;
	susfs_sdcard_mark = mark;
	return 0;
destroy_group:
	fsnotify_destroy_group(group);
put_path:
	path_put(&path);
	return ret;
}

static int susfs_sdcard_probe(void)
{
	struct path path;
	struct inode *inode;
	int ret;

	ret = kern_path(SDCARD_ANDROID_PATH, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &path);
	if (ret)
		return ret;
	inode = d_backing_inode(path.dentry);
	ret = inode && S_ISDIR(inode->i_mode) ? 0 : -ENOTDIR;
	path_put(&path);
	return ret;
}

static void susfs_sdcard_worker(struct work_struct *work)
{
	const struct cred *saved;
	int ret;

	mutex_lock(&susfs_sdcard_lock);
	if (atomic_read(&susfs_sdcard_done))
		goto unlock;
	if (!ksu_susfs_cred_ready() || !ksu_cred)
		goto retry;
	saved = override_creds(ksu_cred);
	ret = susfs_sdcard_probe();
	if (!ret) {
		atomic_set(&susfs_sdcard_done, 1);
		static_branch_disable(&susfs_is_sdcard_android_data_not_decrypted);
		susfs_sdcard_release_watch();
		SUSFS_LOGI("Android directory resolved; sdcard watch complete\n");
	} else {
		if (atomic_xchg(&susfs_sdcard_watch_lost, 0))
			susfs_sdcard_release_watch();
		if (!susfs_sdcard_group) {
			ret = susfs_sdcard_watch_parent();
			if (ret)
				SUSFS_LOGI("sdcard watch not ready: %d\n", ret);
		}
	}
	revert_creds(saved);
	if (!atomic_read(&susfs_sdcard_done))
		goto retry;
	goto unlock;
retry:
	/* Retry readiness/watch failures and missed/pre-existing unlock events. */
	mod_delayed_work(system_unbound_wq, &susfs_sdcard_work, SDCARD_RETRY_DELAY);
unlock:
	mutex_unlock(&susfs_sdcard_lock);
}

void susfs_start_sdcard_monitor_fn(void)
{
	if (!atomic_cmpxchg(&susfs_sdcard_started, 0, 1))
		susfs_sdcard_wake();
}

static void susfs_run_extra_works(struct work_struct *work)
{
	if (!ksu_susfs_cred_ready())
		return;
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	susfs_run_sus_path_loop();
#endif
}
DECLARE_WORK(susfs_extra_works, susfs_run_extra_works);

void susfs_init(void)
{
	/* Work objects are initialized statically before any inline hook can run. */
	SUSFS_LOGI("core foundation initialized: %s\n", SUSFS_VERSION);
}
