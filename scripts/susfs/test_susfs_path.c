/* SPDX-License-Identifier: GPL-2.0 */
/* Whole path object and actual core work/dispatch bodies under host models. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <linux/types.h>
#define __user
#define __rcu
#define pr_debug(...) ((void)0)
#define pr_info(...) ((void)0)
#define pr_err(...) ((void)0)
#define SUSFS_LOGI(...) ((void)0)
#define SUSFS_LOGE(...) ((void)0)
#define likely(x) (x)
#define unlikely(x) (x)
#define __must_check __attribute__((warn_unused_result))
#include "native_overflow.h"
#define GFP_KERNEL 0
#define LOOKUP_FOLLOW 1
#define container_of(p,type,member) ((type *)((char *)(p)-offsetof(type,member)))
struct list_head { struct list_head *next, *prev; };
#define LIST_HEAD(name) struct list_head name = {&name,&name}
#define list_entry(p,type,member) container_of(p,type,member)
#define list_for_each_entry(p,head,member) \
    for(p=list_entry((head)->next,__typeof__(*p),member); &p->member!=(head); p=list_entry(p->member.next,__typeof__(*p),member))
#define WRITE_ONCE(p,v) __atomic_store_n(&(p),(v),__ATOMIC_RELAXED)
#define rcu_assign_pointer(p,v) __atomic_store_n(&(p),(v),__ATOMIC_RELEASE)
static bool __list_add_valid(struct list_head *n, struct list_head *prev, struct list_head *next) { assert(prev->next==next && next->prev==prev); return true; }
#include "native_rculist.h"
static _Thread_local int writer_locks, srcu_depth;
#define DEFINE_MUTEX(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
static void mutex_lock(pthread_mutex_t *m) { assert(!pthread_mutex_lock(m)); writer_locks++; }
static void mutex_unlock(pthread_mutex_t *m) { writer_locks--; assert(!pthread_mutex_unlock(m)); }
struct srcu_struct { atomic_int readers; };
#define DEFINE_STATIC_SRCU(name) static struct srcu_struct name
static _Thread_local struct srcu_struct *active_srcu;
static int srcu_read_lock(struct srcu_struct *s) { assert(!srcu_depth); active_srcu=s; srcu_depth++; atomic_fetch_add(&s->readers,1); return 0; }
static void srcu_read_unlock(struct srcu_struct *s,int idx) { assert(active_srcu==s && srcu_depth==1); srcu_depth--; active_srcu=NULL; atomic_fetch_sub(&s->readers,1); }
static bool srcu_read_lock_held(struct srcu_struct *s) { return active_srcu==s && srcu_depth>0; }
#define __rcu_dereference_check(p,c,q) ({ assert(c); __atomic_load_n(&(p),__ATOMIC_ACQUIRE); })
#include "native_srcu_macros.h"
struct kuid { uid_t val; };
struct address_space { unsigned long flags; };
struct inode { struct address_space *i_mapping; struct kuid i_uid; };
struct dentry { struct inode *inode; };
struct path { void *mnt; struct dentry *dentry; };
struct fs_struct { pthread_mutex_t lock; struct path root; };
struct nsproxy { void *mnt_ns; } init_nsproxy, private_ns;
struct task_struct { struct fs_struct *fs; struct nsproxy *nsproxy; unsigned long flags; };
static struct fs_struct initial_fs, private_fs;
struct task_struct init_task;
static _Thread_local struct task_struct task;
#define current (&task)
static _Thread_local uid_t current_real_uid;
static struct kuid current_uid(void) { return (struct kuid){current_real_uid}; }
static uid_t ksu_get_uid_t(struct kuid uid) { return uid.val; }
static bool uid_eq(struct kuid a,struct kuid b) { return a.val==b.val; }
static int test_thread_flag(int bit) { return !!(current->flags & (1UL<<bit)); }
static void set_thread_flag(int bit) { current->flags |= 1UL<<bit; }
static void clear_thread_flag(int bit) { current->flags &= ~(1UL<<bit); }
static void clear_tsk_thread_flag(struct task_struct *t,int bit) { t->flags &= ~(1UL<<bit); }
static atomic_int held_paths, lookup_calls, scheduled;
static void path_get(const struct path *path) { assert(path->mnt); atomic_fetch_add(&held_paths,1); }
static void path_put(const struct path *path) { assert(path->mnt); assert(atomic_fetch_sub(&held_paths,1)>0); }
#define spin_lock(p) do { assert(!pthread_mutex_lock(p)); } while(0)
#define spin_unlock(p) do { assert(!pthread_mutex_unlock(p)); } while(0)
#include "native_root_helpers.h"
struct qstr { unsigned int len; const char *name; };
#define QSTR_INIT(n,l) {l,n}
struct cred { int tag; } root_cred = {1}, ordinary_cred = {2};
struct cred *ksu_cred = &root_cred;
static _Thread_local const struct cred *active_cred=&ordinary_cred;
static bool ready;
bool ksu_susfs_cred_ready(void) { return ready; }
static const struct cred *override_creds(const struct cred *c) { assert(c==&root_cred); const struct cred *saved=active_cred; active_cred=c; return saved; }
static void revert_creds(const struct cred *c) { assert(active_cred==&root_cred); active_cred=c; }
struct work_struct { void (*fn)(struct work_struct *); };
#define DECLARE_WORK(name,fn) struct work_struct name = {fn}
static bool schedule_work(struct work_struct *work) { assert(!writer_locks); atomic_fetch_add(&scheduled,1); return true; }
static atomic_int live_allocations, allocation_calls;
static int fail_allocation;
static void *kzalloc(size_t size,int flags) { int n=atomic_fetch_add(&allocation_calls,1)+1; if(n==fail_allocation) return NULL; void *p=calloc(1,size); assert(p); atomic_fetch_add(&live_allocations,1); return p; }
static void kfree(void *p) { if(p) { assert(atomic_fetch_sub(&live_allocations,1)>0); free(p); } }
static _Thread_local char *user_buffer;
static _Thread_local size_t user_size;
static _Thread_local bool can_read, can_write;
static size_t copy_from_user(void *dst,const void *src,size_t size) {
    uintptr_t at=(uintptr_t)src,base=(uintptr_t)user_buffer;
    if(!can_read || !src || at<base || at-base>user_size || size>user_size-(at-base)) return size;
    memcpy(dst,src,size); return 0;
}
static size_t copy_to_user(void *dst,const void *src,size_t size) {
    uintptr_t at=(uintptr_t)dst,base=(uintptr_t)user_buffer;
    if(!can_write || !dst || at<base || at-base>user_size || size>user_size-(at-base)) return size;
    memcpy(dst,src,size); return 0;
}
static ssize_t strscpy(char *dst,const char *src,size_t size) { if(!size)return -E2BIG; size_t n=strnlen(src,size); size_t copied=n<size?n:size-1; memcpy(dst,src,copied); dst[copied]=0; return n<size ? (ssize_t)n : -E2BIG; }
static void set_bit(unsigned int bit,unsigned long *flags) { __atomic_fetch_or(flags,1UL<<bit,__ATOMIC_RELAXED); }
static int test_bit(unsigned int bit,const unsigned long *flags) { return !!(__atomic_load_n(flags,__ATOMIC_RELAXED) & (1UL<<bit)); }
static struct address_space mappings[8];
static struct inode inodes[8];
static struct dentry dentries[8];
static int lookup_error;
static bool null_inode, missing_mapping, permit_ordinary;
static _Thread_local char last_path[256];
static pthread_barrier_t io_enter, io_leave;
static bool block_lookup;
static int kern_path(const char *name,int flags,struct path *out) {
    assert(!writer_locks && flags==LOOKUP_FOLLOW);
    if(srcu_depth) assert(active_cred==&root_cred);
    else if(!permit_ordinary && active_cred!=&root_cred) return -EACCES;
    atomic_fetch_add(&lookup_calls,1);
    assert(strlen(name)<sizeof(last_path)); strcpy(last_path,name);
    if(block_lookup && srcu_depth) { block_lookup=false; pthread_barrier_wait(&io_enter); pthread_barrier_wait(&io_leave); }
    if(lookup_error) return lookup_error;
    unsigned int index=0;
    if(!strcmp(name,"/late")) index=1;
    if(!strcmp(name,"/second")) index=2;
    if(!strcmp(name,"/data")) index=3;
    *out=(struct path){(void *)1,&dentries[index]}; path_get(out); return 0;
}
static struct inode unmapped_inode;
static struct inode *d_backing_inode(struct dentry *dentry) { return null_inode ? NULL : (missing_mapping ? &unmapped_inode : dentry->inode); }
#include "path_object.h"
#include "core_command_work.h"
#include "pinned_dispatch.h"
#include "reboot_magic.h"
static int ksu_install_fd(void) { return -ENOMEM; }
static void ksu_close_fd(int fd) {}
#include "pinned_transport.h"
struct fuse_inode { unsigned long prefix; struct inode inode; };
#include "native_fuse_identity.h"
static LIST_HEAD(LH_SUS_PATH_LOOP);
static DEFINE_MUTEX(susfs_mutex_lock_sus_path);
#include "pinned_path_negative.h"
static void task_init(void) { current_real_uid=0; current->fs=&initial_fs; current->nsproxy=&init_nsproxy; current->flags=0; active_cred=&ordinary_cred; can_read=can_write=true; }
static void discard_registry(void) {
    assert(!atomic_load(&susfs_path_srcu.readers));
    struct list_head *node=susfs_path_entries.next;
    while(node!=&susfs_path_entries) { struct list_head *next=node->next; kfree(list_entry(node,struct susfs_path_entry,node)); node=next; }
    susfs_path_entries.next=susfs_path_entries.prev=&susfs_path_entries; susfs_path_entry_count=0;
    assert(!atomic_load(&live_allocations) && !atomic_load(&held_paths));
}
static void reset(void) {
    discard_registry(); task_init(); ready=true; ksu_cred=&root_cred;
    lookup_error=fail_allocation=0; null_inode=missing_mapping=block_lookup=false; permit_ordinary=true;
    atomic_store(&allocation_calls,0); atomic_store(&lookup_calls,0); atomic_store(&scheduled,0);
    memset(mappings,0,sizeof(mappings));
    for(int i=0;i<8;i++) { inodes[i].i_mapping=&mappings[i]; inodes[i].i_uid.val=0; dentries[i].inode=&inodes[i]; }
}
static int request(unsigned int cmd,const char *name) {
    struct st_susfs_sus_path info={0}; if(name)strcpy(info.target_pathname,name); info.err=126;
    void *arg=&info; user_buffer=(char *)&info; user_size=sizeof(info);
    assert(!ksu_handle_sys_reboot(KSU_INSTALL_MAGIC1,SUSFS_MAGIC,cmd,&arg)); return info.err;
}
static void immediate_tests(void) {
    reset(); unsigned long base=1UL<<5; mappings[0].flags=base;
    assert(!request(CMD_SUSFS_ADD_SUS_PATH,"/file")); assert(mappings[0].flags==(base | (1UL<<AS_FLAGS_SUS_PATH)) && !atomic_load(&held_paths));
    assert(!request(CMD_SUSFS_ADD_SUS_PATH,"relative")); assert(!strcmp(last_path,"relative"));
    assert(request(CMD_SUSFS_ADD_SUS_PATH,"")==-EINVAL);
    struct st_susfs_sus_path info; memset(&info,'A',sizeof(info)); info.err=126;
    void *arg=&info; user_buffer=(char *)&info; user_size=sizeof(info); int calls=atomic_load(&lookup_calls);
    ksu_handle_susfs_cmd(CMD_SUSFS_ADD_SUS_PATH,&arg); assert(info.err==-ENAMETOOLONG && atomic_load(&lookup_calls)==calls);
    memset(&info,0,sizeof(info)); strcpy(info.target_pathname,"/file"); info.err=126; can_read=false;
    ksu_handle_susfs_cmd(CMD_SUSFS_ADD_SUS_PATH,&arg); assert(info.err==-EFAULT && atomic_load(&lookup_calls)==calls); can_read=true;
    info.err=126; mappings[0].flags=0; can_write=false;
    ksu_handle_susfs_cmd(CMD_SUSFS_ADD_SUS_PATH,&arg); assert(info.err==126 && test_bit(AS_FLAGS_SUS_PATH,&mappings[0].flags)); can_write=true;
    arg=(void *)(UINTPTR_MAX-8); ksu_handle_susfs_cmd(CMD_SUSFS_ADD_SUS_PATH,&arg); arg=&info;
    current_real_uid=10001; info.err=126; mappings[0].flags=0;
    ksu_handle_sys_reboot(KSU_INSTALL_MAGIC1,SUSFS_MAGIC,CMD_SUSFS_ADD_SUS_PATH,&arg); assert(info.err==126 && !mappings[0].flags); current_real_uid=0;
    int errors[]={-ENOENT,-EACCES,-ELOOP,-ENAMETOOLONG,-EIO};
    for(size_t i=0;i<sizeof(errors)/sizeof(*errors);i++) { lookup_error=errors[i]; assert(request(CMD_SUSFS_ADD_SUS_PATH,"/file")==errors[i]); assert(!atomic_load(&held_paths)); }
    lookup_error=0; null_inode=true; assert(request(CMD_SUSFS_ADD_SUS_PATH,"/file")==-ENOENT && !atomic_load(&held_paths));
    null_inode=false; missing_mapping=true; assert(request(CMD_SUSFS_ADD_SUS_PATH,"/file")==-ENOENT && !atomic_load(&held_paths)); missing_mapping=false;
    permit_ordinary=false; assert(request(CMD_SUSFS_ADD_SUS_PATH,"/file")==-EACCES && active_cred==&ordinary_cred);
    permit_ordinary=true; struct path path; assert(!susfs_get_data_path(&path)); assert(atomic_load(&held_paths)==1); path_put(&path);
    struct fuse_inode fi={0}; fi.inode.i_mapping=&mappings[4]; fi.inode.i_uid.val=0;
    assert(&get_fuse_inode(&fi.inode)->inode==&fi.inode);
    set_bit(AS_FLAGS_SUS_PATH,&fi.inode.i_mapping->flags); current_real_uid=10001; set_thread_flag(TIF_PROC_UMOUNTED);
    assert(susfs_is_inode_sus_path(&fi.inode));
    inodes[0].i_mapping=&mappings[0]; set_bit(AS_FLAGS_SUS_PATH,&mappings[0].flags);
    assert(susfs_is_inode_sus_path(&inodes[0])); inodes[0].i_uid.val=10001; assert(!susfs_is_inode_sus_path(&inodes[0]));
    inodes[0].i_uid.val=0; clear_thread_flag(TIF_PROC_UMOUNTED); assert(!susfs_is_inode_sus_path(&inodes[0])); set_thread_flag(TIF_PROC_UMOUNTED);
    current_real_uid=0; assert(!susfs_is_inode_sus_path(&inodes[0])); current_real_uid=1053; assert(!susfs_is_inode_sus_path(&inodes[0]));
    current_real_uid=1010001; assert(susfs_is_inode_sus_path(&inodes[0])); inodes[0].i_uid.val=1010001; assert(!susfs_is_inode_sus_path(&inodes[0]));
    assert(!susfs_is_inode_sus_path(NULL)); inodes[0].i_mapping=NULL; assert(!susfs_is_inode_sus_path(&inodes[0])); inodes[0].i_mapping=&mappings[0];
    mappings[0].flags=0; assert(!susfs_is_inode_sus_path(&inodes[0])); task_init();
}
static void loop_tests(void) {
    reset(); assert(request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"")==-EINVAL); assert(request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"relative")==-EINVAL);
    current->nsproxy=&private_ns; assert(request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late")==-EXDEV && !susfs_path_entry_count); current->nsproxy=&init_nsproxy;
    current->fs=&private_fs; assert(request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late")==-EXDEV && !atomic_load(&held_paths)); current->fs=&initial_fs;
    current->fs=NULL; assert(request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late")==-EXDEV); current->fs=&initial_fs;
    void *initial_namespace=init_nsproxy.mnt_ns; init_nsproxy.mnt_ns=NULL; assert(request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late")==-EXDEV); init_nsproxy.mnt_ns=initial_namespace;
    fail_allocation=1; assert(request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late")==-ENOMEM && !susfs_path_entry_count && !atomic_load(&held_paths)); fail_allocation=0;
    ready=false; assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late") && susfs_path_entry_count==1 && !atomic_load(&scheduled));
    susfs_extra_works.fn(&susfs_extra_works); assert(!atomic_load(&lookup_calls)); ready=true;
    int allocations=atomic_load(&allocation_calls); fail_allocation=allocations+1;
    assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late") && atomic_load(&allocation_calls)==allocations && atomic_load(&scheduled)==1); fail_allocation=0;
    lookup_error=-ENOENT; susfs_extra_works.fn(&susfs_extra_works); assert(!mappings[1].flags && active_cred==&ordinary_cred && !atomic_load(&held_paths)); lookup_error=0;
    susfs_extra_works.fn(&susfs_extra_works); assert(test_bit(AS_FLAGS_SUS_PATH,&mappings[1].flags));
    mappings[1].flags=0; susfs_extra_works.fn(&susfs_extra_works); assert(test_bit(AS_FLAGS_SUS_PATH,&mappings[1].flags));
    current->nsproxy=&private_ns; int calls=atomic_load(&lookup_calls); susfs_extra_works.fn(&susfs_extra_works); assert(atomic_load(&lookup_calls)==calls); current->nsproxy=&init_nsproxy;
    current->fs=&private_fs; susfs_extra_works.fn(&susfs_extra_works); assert(atomic_load(&lookup_calls)==calls); current->fs=&initial_fs;
    ksu_cred=NULL; susfs_extra_works.fn(&susfs_extra_works); assert(atomic_load(&lookup_calls)==calls); ksu_cred=&root_cred;
    char longest[256]; longest[0]='/'; memset(longest+1,'b',254); longest[255]=0;
    assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,longest));
    struct susfs_path_entry *entry=list_entry(susfs_path_entries.prev,struct susfs_path_entry,node); assert(strlen(entry->pathname)==255 && !strcmp(entry->pathname,longest));
    for(unsigned int i=susfs_path_entry_count;i<SUSFS_PATH_LOOP_MAX_ENTRIES;i++) { char name[64]; snprintf(name,sizeof(name),"/limit/%u",i); assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,name)); }
    assert(susfs_path_entry_count==1024 && request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/overflow")==-E2BIG);
    allocations=atomic_load(&allocation_calls); assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late") && atomic_load(&allocation_calls)==allocations);
    discard_registry();
}
static void *worker_thread(void *arg) { task_init(); for(int i=0;i<20;i++) susfs_extra_works.fn(&susfs_extra_works); assert(active_cred==&ordinary_cred && !srcu_depth); return NULL; }
static void *writer_thread(void *arg) { task_init(); assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/shared")); unsigned int n=(unsigned int)(uintptr_t)arg; for(int i=0;i<40;i++) { char name[64]; snprintf(name,sizeof(name),"/race/%u/%d",n,i); assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,name)); assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,name)); } return NULL; }
static void *blocked_worker(void *arg) { task_init(); susfs_extra_works.fn(&susfs_extra_works); return NULL; }
static void concurrent_tests(void) {
    reset(); assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/late")); block_lookup=true;
    pthread_barrier_init(&io_enter,NULL,2); pthread_barrier_init(&io_leave,NULL,2); pthread_t blocked;
    assert(!pthread_create(&blocked,NULL,blocked_worker,NULL)); pthread_barrier_wait(&io_enter);
    assert(!request(CMD_SUSFS_ADD_SUS_PATH_LOOP,"/second")); /* Must not wait for reader IO. */
    pthread_barrier_wait(&io_leave); assert(!pthread_join(blocked,NULL));
    assert(test_bit(AS_FLAGS_SUS_PATH,&mappings[1].flags) && test_bit(AS_FLAGS_SUS_PATH,&mappings[2].flags));
    pthread_barrier_destroy(&io_enter); pthread_barrier_destroy(&io_leave);
    pthread_t threads[8];
    for(int i=0;i<8;i++) assert(!pthread_create(&threads[i],NULL,i<4?writer_thread:worker_thread,(void *)(uintptr_t)i));
    for(int i=0;i<8;i++) assert(!pthread_join(threads[i],NULL));
    assert(susfs_path_entry_count==163 && atomic_load(&live_allocations)==163 && !atomic_load(&held_paths));
    struct st_susfs_enabled_features features; memset(&features,'X',sizeof(features)); void *arg=&features; user_buffer=(char *)&features; user_size=sizeof(features); ksu_handle_susfs_cmd(CMD_SUSFS_SHOW_ENABLED_FEATURES,&arg);
    assert(!features.err && !strcmp(features.enabled_features,"CONFIG_KSU_SUSFS_SUS_PATH\n"));
    discard_registry();
}
static void negative_control(void) {
    reset(); char name[256]; name[0]='/'; memset(name+1,'q',254); name[255]=0;
    struct st_susfs_sus_path info={0}; strcpy(info.target_pathname,name); info.err=126;
    user_buffer=(char *)&info; user_size=sizeof(info); void *arg=&info;
    pinned_add_sus_path_loop(&arg); assert(!info.err);
    struct st_susfs_sus_path_list *entry=list_entry(LH_SUS_PATH_LOOP.next,struct st_susfs_sus_path_list,list);
    assert(strlen(entry->target_pathname)==254 && strcmp(entry->target_pathname,name));
    kfree(entry); LH_SUS_PATH_LOOP.next=LH_SUS_PATH_LOOP.prev=&LH_SUS_PATH_LOOP;
    assert(!atomic_load(&live_allocations));
    puts("EXPECTED NEGATIVE: actual pinned loop accepts a 255-byte name but silently stores a different 254-byte prefix");
}
int main(void) {
    assert(!pthread_mutex_init(&initial_fs.lock,NULL) && !pthread_mutex_init(&private_fs.lock,NULL));
    initial_fs.root=(struct path){(void *)1,&dentries[7]}; private_fs.root=(struct path){(void *)1,&dentries[6]};
    init_task.fs=&initial_fs; init_task.nsproxy=&init_nsproxy; init_nsproxy.mnt_ns=(void *)1; private_ns.mnt_ns=(void *)2;
    immediate_tests(); loop_tests(); concurrent_tests(); negative_control();
    assert(!atomic_load(&held_paths) && !atomic_load(&live_allocations));
    pthread_mutex_destroy(&initial_fs.lock); pthread_mutex_destroy(&private_fs.lock);
    puts("PASS: whole path core with real transport/dispatch and work consumer; bounded payloads, mapping/UID predicates, namespace/root checks, append-only SRCU publication and concurrent retries under host models");
    return 0;
}
