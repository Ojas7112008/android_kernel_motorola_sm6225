/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#define __user
#define likely(x) (x)
#define unlikely(x) (x)
#define GFP_KERNEL 0
#define PAGE_SIZE 4096
#define min_t(t,a,b) ((t)(a)<(t)(b)?(t)(a):(t)(b))
#define ERR_PTR(err) ((void *)(intptr_t)(err))
#define PTR_ERR(p) ((int)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p)>=(uintptr_t)-4095)
#define READ_ONCE(x) (x)
#define LOOKUP_RCU 1
#define LOOKUP_REVAL 2
#define IOP_DEFAULT_READLINK 1
#define DEFINE_DELAYED_CALL(n) int n=0
#define spin_lock(p) do {} while(0)
#define spin_unlock(p) do {} while(0)
static unsigned long task_flags;
static unsigned int uid;
static bool domain,copy_fault,alloc_fault,name_fault;
struct kuid_model { unsigned int val; };
static struct kuid_model current_uid(void) { return (struct kuid_model){uid}; }
#define test_thread_flag(n) (!!(task_flags&(1UL<<(n))))
static bool susfs_is_current_ksu_domain(void) { return domain; }
static bool test_bit(int n,const unsigned long *p) { return !!(*p&(1UL<<n)); }
static void set_bit(int n,unsigned long *p) { *p|=1UL<<n; }
static int allocations,rcu_depth,writer_depth;
static void *deferred[100]; static int ndeferred;
static void *kzalloc(size_t n,int flags) { if(alloc_fault)return NULL; allocations++; return calloc(1,n); }
static void kfree(void *p) { if(p) { allocations--; free(p); } }
static void flush_rcu(void) { if(!rcu_depth)while(ndeferred)kfree(deferred[--ndeferred]); }
static void rcu_read_lock(void) { rcu_depth++; }
static void rcu_read_unlock(void) { assert(rcu_depth); rcu_depth--; flush_rcu(); }
#define kfree_rcu(p,member) do { deferred[ndeferred++]=(p); flush_rcu(); } while(0)
#define DEFINE_MUTEX(n) int n
#define mutex_lock(p) do { assert(!*(p)); *(p)=1; writer_depth++; } while(0)
#define mutex_unlock(p) do { assert(*(p)); *(p)=0; writer_depth--; } while(0)
struct list_head { struct list_head *next,*prev; };
struct rcu_head { int unused; };
#define LIST_HEAD(n) struct list_head n={&n,&n}
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define list_for_each_entry(obj,head,m) \
 for((obj)=container_of((head)->next,__typeof__(*(obj)),m); &(obj)->m!=(head); (obj)=container_of((obj)->m.next,__typeof__(*(obj)),m))
#define list_for_each_entry_rcu list_for_each_entry
static void list_add_rcu(struct list_head *node,struct list_head *head)
{ assert(writer_depth); node->next=head->next; node->prev=head; head->next->prev=node; head->next=node; }
static void list_replace_rcu(struct list_head *old,struct list_head *node)
{ assert(writer_depth); node->next=old->next; node->prev=old->prev; old->prev->next=node; old->next->prev=node; }
static size_t copy_from_user(void *dst,const void *src,size_t n) { if(copy_fault||!src)return n; memcpy(dst,src,n); return 0; }
static size_t copy_to_user(void *dst,const void *src,size_t n) { if(copy_fault||!dst)return n; memcpy(dst,src,n); return 0; }
struct super_block { dev_t s_dev; unsigned long s_magic; };
struct address_space { unsigned long flags; };
struct dentry;
struct inode;
struct inode_operations { int (*readlink)(struct dentry *,char *,int); const char *(*get_link)(struct dentry *,struct inode *,int *); };
struct inode { unsigned long i_ino; struct super_block *i_sb; struct address_space *i_mapping; struct inode_operations *i_op; unsigned i_opflags; int i_lock; const char *i_link; bool symlink; };
struct dentry { struct inode *inode; const char *name; };
struct path { struct dentry *dentry; };
struct filename { char *name; };
struct file { int dummy; };
struct open_flags { int lookup_flags; };
struct nameidata { struct filename *name; struct nameidata *saved; int dfd,total_link_count; int *stack,internal[2]; struct filename **susfs_redirect; };
static struct { struct nameidata *nameidata; } model_current;
#define current (&model_current)
static struct dentry dentries[3]; static struct inode inodes[3];
static int path_refs,lookup_fail_index=-1,lookup_error=-EACCES,names,cleanup;
static struct inode *d_backing_inode(struct dentry *d) { return d->inode; }
static struct inode *d_inode(struct dentry *d) { return d->inode; }
static bool d_is_symlink(struct dentry *d) { return d->inode->symlink; }
static int kern_path(const char *name,int flags,struct path *path)
{ assert(flags==0 && strlen(name)<256); for(int i=0;i<3;i++)if(!strcmp(name,dentries[i].name)) { if(i==lookup_fail_index)return lookup_error; path->dentry=&dentries[i]; path_refs++; return 0; } return -ENOENT; }
static void path_put(struct path *p) { assert(p->dentry && path_refs>0); path_refs--; }
static struct filename *getname_kernel(const char *name)
{ assert(!rcu_depth); if(name_fault)return ERR_PTR(-ENOMEM); struct filename *f=malloc(sizeof(*f)); f->name=strdup(name); names++; return f; }
static void putname(struct filename *name) { assert(name && !IS_ERR(name)); names--; free(name->name); free(name); }
static void do_delayed_call(int *done) { if(*done)cleanup++; }
static int native_link_error;
static const char *get_link_model(struct dentry *d,struct inode *i,int *done)
{ *done=1; return native_link_error?ERR_PTR(native_link_error):"/native-link"; }
static int readlink_copy(char *buffer,int len,const char *link)
{ int n=min_t(size_t,strlen(link),len); return copy_to_user(buffer,link,n)?-EFAULT:n; }
static unsigned long __get_free_page(int flags) { return (unsigned long)calloc(1,PAGE_SIZE); }
static void free_page(unsigned long page) { free((void *)page); }
static char *d_path(struct path *path,char *tmp,size_t n)
{ const char *name=path->dentry->name; size_t len=strlen(name); char *p=tmp+n-len-1; memcpy(p,name,len+1); return p; }

#include "linux/susfs.h"
#include "linux/susfs_redirect.h"
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
#include "redirect_handler.h"
#include "native_resolver.h"
#endif
#include "native_nameidata.h"
static int attempts,original_effects,replacement_effects;
static int original_rcu_fail,original_stale_fail,replacement_rcu_fail,replacement_error;
static struct file opened;
static struct file *path_openat(struct nameidata *nd,const struct open_flags *op,unsigned flags)
{
	attempts++;
	bool replacement=!strcmp(nd->name->name,"/replacement");
	if(!replacement && original_rcu_fail && (flags&LOOKUP_RCU)) { original_rcu_fail=0; return ERR_PTR(-ECHILD); }
	if(!replacement && original_stale_fail && !(flags&LOOKUP_REVAL)) { original_stale_fail=0; return ERR_PTR(-ESTALE); }
	if(replacement && replacement_rcu_fail && (flags&LOOKUP_RCU)) { replacement_rcu_fail=0; return ERR_PTR(-ECHILD); }
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	struct path path={.dentry=replacement?&dentries[1]:&dentries[0]};
	int ret=susfs_redirect_resolved_path(nd,&path); if(ret)return ERR_PTR(ret);
#endif
	if(replacement) { if(replacement_error)return ERR_PTR(replacement_error); replacement_effects++; }
	else original_effects++;
	return &opened;
}
#include "native_wrapper.h"
#include "native_readlink.h"
#include "native_proc_readlink.h"

#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
static void add_rule(int scheme,const char *target,const char *replacement,int error)
{
	struct st_susfs_open_redirect info={.uid_scheme=scheme,.err=99};
	snprintf(info.target_pathname,sizeof(info.target_pathname),"%s",target);
	snprintf(info.redirected_pathname,sizeof(info.redirected_pathname),"%s",replacement);
	void *p=&info; susfs_add_open_redirect(&p); assert(info.err==error && !path_refs);
}
static void policies(void)
{
	for(int scheme=0;scheme<=4;scheme++) {
		add_rule(scheme,"/target","/replacement",0);
		unsigned uids[]={0,1000,10000,110000,99000};
		for(size_t i=0;i<sizeof(uids)/sizeof(uids[0]);i++)for(int root=0;root<2;root++)for(int umount=0;umount<2;umount++) {
			uid=uids[i]; domain=root; task_flags=umount?(1UL<<TIF_PROC_UMOUNTED):0;
			bool expected=scheme==0?uid%100000<10000:scheme==1?uid==0&&!domain:scheme==2?!domain:scheme==3?umount&&uid>=10000:umount;
			struct filename *name=susfs_redirect_filename(&inodes[0]);
			assert(!!name==expected && susfs_redirect_may_apply()==expected);
			if(name) { assert(!IS_ERR(name) && !strcmp(name->name,"/replacement")); putname(name); }
		}
	}
	uid=10000; domain=false; task_flags=1UL<<TIF_PROC_UMOUNTED;
}
static void lifetime_and_rules(void)
{
	add_rule(UID_UMOUNTED_APP_PROC,"/target","/replacement",0);
	add_rule(UID_UMOUNTED_APP_PROC,"/other","/replacement",-EEXIST);
	add_rule(UID_UMOUNTED_APP_PROC,"/replacement","/other",-EEXIST);
	add_rule(UID_UMOUNTED_APP_PROC,"/target","/target",-EINVAL);
	add_rule(99,"/target","/replacement",-EINVAL);
	add_rule(0,"relative","/replacement",-EINVAL);
	inodes[1].i_sb->s_magic=FUSE_SUPER_MAGIC; add_rule(0,"/target","/replacement",-EOPNOTSUPP); inodes[1].i_sb->s_magic=0;
	lookup_fail_index=1; add_rule(0,"/target","/replacement",-EACCES); lookup_fail_index=-1;
	alloc_fault=true; add_rule(0,"/target","/replacement",-ENOMEM); alloc_fault=false;
	name_fault=true; assert(susfs_redirect_filename(&inodes[0])==ERR_PTR(-ENOMEM)); name_fault=false;
	struct susfs_redirect_view view; assert(susfs_redirect_view(&inodes[1],&view) && view.ino==inodes[0].i_ino);
	add_rule(UID_UMOUNTED_APP_PROC,"/target","/other",0);
	assert(!strcmp(view.name,"/target") && !susfs_redirect_view(&inodes[1],&view));
	assert(susfs_redirect_view(&inodes[2],&view));
	add_rule(UID_UMOUNTED_APP_PROC,"/target","/replacement",0);
	struct inode reused=inodes[0]; struct address_space fresh={0}; reused.i_mapping=&fresh;
	assert(!susfs_redirect_filename(&reused));
	assert(allocations==1 && !names && !rcu_depth && !writer_depth);
}
static void wrapper(void)
{
	struct filename *original=getname_kernel("/target"); struct open_flags flags={0};
	original_rcu_fail=original_stale_fail=replacement_rcu_fail=1;
	assert(do_filp_open(123,original,&flags)==&opened);
	assert(attempts==5 && !original_effects && replacement_effects==1 && names==1 && !current->nameidata);
	replacement_error=-EACCES;
	assert(do_filp_open(123,original,&flags)==ERR_PTR(-EACCES));
	assert(!original_effects && replacement_effects==1 && names==1 && !current->nameidata);
	replacement_error=0; name_fault=true;
	assert(do_filp_open(123,original,&flags)==ERR_PTR(-ENOMEM) && names==1); name_fault=false;
	putname(original); assert(!names);
}
static void links(void)
{
	char buffer[32]; memset(buffer,'X',sizeof(buffer));
	assert(susfs_redirect_readlink(&inodes[1],buffer,3)==3 && !memcmp(buffer,"/ta",3) && buffer[3]=='X');
	assert(susfs_redirect_readlink(&inodes[1],buffer,32)==7 && !memcmp(buffer,"/target",7));
	assert(susfs_redirect_readlink(&inodes[1],buffer,0)==-EINVAL);
	copy_fault=true; assert(susfs_redirect_readlink(&inodes[1],buffer,3)==-EFAULT); copy_fault=false;
	struct path path={.dentry=&dentries[1]}; memset(buffer,'X',sizeof(buffer));
	assert(do_proc_readlink(&path,buffer,3)==3 && !memcmp(buffer,"/ta",3));
	inodes[1].symlink=true; assert(vfs_readlink(&dentries[1],buffer,3)==3 && cleanup==1);
	native_link_error=-EACCES; assert(vfs_readlink(&dentries[1],buffer,32)==-EACCES); native_link_error=0;
	uid=0; assert(susfs_redirect_readlink(&inodes[1],buffer,3)==-ENOENT);
	assert(do_proc_readlink(&path,buffer,32)==12 && !memcmp(buffer,"/replacement",12)); uid=10000;
}
#endif
int main(void)
{
	struct super_block sb[3]={0}; struct address_space mappings[3]={0}; struct inode_operations ops={.get_link=get_link_model};
	const char *paths[]={"/target","/replacement","/other"};
	for(int i=0;i<3;i++) { sb[i].s_dev=i+1; inodes[i]=(struct inode){.i_ino=i+10,.i_sb=&sb[i],.i_mapping=&mappings[i],.i_op=&ops}; dentries[i]=(struct dentry){.inode=&inodes[i],.name=paths[i]}; }
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	policies(); lifetime_and_rules(); wrapper(); links();
	struct list_head *p=susfs_redirect_rules.next;
	while(p!=&susfs_redirect_rules) { struct list_head *next=p->next; kfree(container_of(p,struct susfs_redirect_rule,list)); p=next; }
	flush_rcu(); assert(!allocations && !names && !path_refs && !current->nameidata);
#else
	struct filename *name=getname_kernel("/target"); struct open_flags flags={0};
	assert(do_filp_open(123,name,&flags)==&opened && original_effects==1); putname(name);
#endif
	puts("five UID schemes, rule conflicts/lifetime, owned-name retry/permission errors and readlink truncation/faults passed under host models");
}
