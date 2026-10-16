/* SPDX-License-Identifier: GPL-2.0 */
/* Extracted native/provider functions with explicit host lifetime/IO mocks. */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "linux/types.h"
#define __user
#define __exit
#define __read_mostly
#define likely(v) (v)
#define unlikely(v) (v)
#define GFP_KERNEL 0
#define SZ_1M (1024*1024)
#define ERR_PTR(v) ((void *)(intptr_t)(v))
#define PTR_ERR(p) ((long)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p)>=(uintptr_t)-4095)
#define IS_ERR_OR_NULL(p) (!(p)||IS_ERR(p))
#define min_t(t,a,b) ({t x=(a),y=(b);x<y?x:y;})
#define pr_info(...) ((void)0)
#define pr_warn(...) ((void)0)
#define pr_err(...) ((void)0)
#define BUG_ON(v) assert(!(v))
#define DEFINE_MUTEX(v) pthread_mutex_t v=PTHREAD_MUTEX_INITIALIZER
#define mutex_lock(p) assert(!pthread_mutex_lock(p))
#define mutex_unlock(p) assert(!pthread_mutex_unlock(p))
#define READ 0
#define VERIFY_WRITE 1
#define MAX_RW_COUNT 4096
#define RWF_HIPRI 1
#define KSTAT_QUERY_FLAGS 0x6000
#define EIOCBQUEUED 529
#define FMODE_READ 1
#define FMODE_WRITE 2
#define FMODE_LSEEK 4
#define FMODE_CAN_READ 0x20000
#define FMODE_OPENED 0x80000
#define FMODE_STREAM 0x200000
#define KERNEL_SU_DOMAIN "su"
#define KSUD_PATH "/data/adb/ksud"
struct list_head {struct list_head *next,*prev;};
#define LIST_HEAD(n) struct list_head n={&n,&n}
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define list_for_each_entry(p,h,m) for(p=container_of((h)->next,__typeof__(*p),m);&(p)->m!=(h);p=container_of((p)->m.next,__typeof__(*p),m))
static void list_add(struct list_head *n,struct list_head *h){n->next=h->next;n->prev=h;h->next->prev=n;h->next=n;}
static void list_del(struct list_head *n){n->prev->next=n->next;n->next->prev=n->prev;}
static bool list_empty(struct list_head *h){return h->next==h;}
struct cred {bool init;u32 uid;};
static const struct cred init_cred={true,0},priv_cred={false,0},other_cred={false,10001};
static _Thread_local const struct cred *subjective=&init_cred;
static const struct cred *ksu_cred=&priv_cred;
typedef struct {u32 val;} kuid_t;
static kuid_t current_uid(void){return (kuid_t){subjective->uid};}
static const struct cred *current_cred(void){return subjective;}
static bool is_init(const struct cred *c){return c->init;}
struct task_struct {int pid;char comm[16];unsigned long flags;};
static struct task_struct task={.pid=1,.comm="init"};
#define current (&task)
#define BIT(n) (1UL<<(n))
static bool test_thread_flag(unsigned n){return !!(task.flags&BIT(n));}
static void set_thread_flag(unsigned n){task.flags|=BIT(n);}
static void clear_thread_flag(unsigned n){task.flags&=~BIT(n);}
static void clear_tsk_thread_flag(struct task_struct *t,unsigned n){t->flags&=~BIT(n);}
#include "linux/ksu_susfs.h"
struct super_block {loff_t s_maxbytes;};
struct inode {unsigned int i_mode;loff_t size;struct super_block *i_sb;};
struct dentry {struct inode *d_inode;struct {const char *name;} d_name;};
struct path {struct dentry *dentry;const char *full;};
typedef unsigned rwf_t;
struct iov_iter {struct iovec *iov;size_t nr_segs,count,offset;};
struct kiocb {struct file *ki_filp;loff_t ki_pos;bool sync;};
struct file_operations {ssize_t (*read)(struct file *,char *,size_t,loff_t *);ssize_t (*read_iter)(struct kiocb *,struct iov_iter *);loff_t (*llseek)(struct file *,loff_t,int);ssize_t (*write)(struct file *,const char *,size_t,loff_t *);};
struct file {struct path f_path;unsigned f_mode;int f_flags;loff_t f_pos;unsigned long f_version;pthread_mutex_t f_lock;const struct file_operations *f_op;void *private_data;const char *data;size_t length;int refs;};
struct fd {struct file *file;};
struct kstat {loff_t size;int other;u32 result_mask;};
static bool ready=true,ksu_no_custom_rc,deny_access,omit_size,init_rc_enabled=true;
#define ksu_init_rc_hook_inactive() (!init_rc_enabled)
static int rw_error,getattr_error,native_error,opens,closes,override_count,revert_count,read_notifications,stat_calls;
static int first_open_error,second_open_error,module_error,alloc_attempts,fail_alloc_at,live_allocations;
static size_t module_chunk,copy_budget=SIZE_MAX;
static bool force_early_eof;
static struct file *fd_file;
static struct file rc[2],module_file;
static struct inode inodes[3];static struct dentry dentries[3];static struct super_block sb={LLONG_MAX};
static const char base_data[]="on boot\n    setprop devon.test 1\n",module_data[]="\non boot\n    setprop devon.module 1\n";
static bool access_ok(int type,const void *buf,size_t count){return !deny_access && (!count||buf);}
static int rw_verify_area(int type,struct file *file,loff_t *pos,size_t count){return rw_error;}
static void fsnotify_access(struct file *file){read_notifications++;}
static void add_rchar(struct task_struct *t,ssize_t n){}
static void inc_syscr(struct task_struct *t){}
static size_t copy_to_user(void *dst,const void *src,size_t n){size_t copied=min_t(size_t,n,copy_budget);if(copied)memcpy(dst,src,copied);if(copy_budget!=SIZE_MAX)copy_budget-=copied;return n-copied;}
static size_t iov_iter_count(const struct iov_iter *i){return i->count;}
static void iov_iter_truncate(struct iov_iter *i,size_t n){if(i->count>n)i->count=n;}
static void iov_iter_reexpand(struct iov_iter *i,size_t n){i->count=n;}
static struct iovec iov_iter_iovec(struct iov_iter *i){return (struct iovec){(char *)i->iov->iov_base+i->offset,min_t(size_t,i->count,i->iov->iov_len-i->offset)};}
static void iov_iter_advance(struct iov_iter *i,size_t n){assert(n<=i->count);i->count-=n;while(n){size_t k=min_t(size_t,n,i->iov->iov_len-i->offset);i->offset+=k;n-=k;if(i->offset==i->iov->iov_len){i->iov++;i->nr_segs--;i->offset=0;}}}
static size_t copy_to_iter(const void *src,size_t n,struct iov_iter *i){size_t done=0;n=min_t(size_t,n,i->count);while(done<n){struct iovec v=iov_iter_iovec(i);size_t k=min_t(size_t,v.iov_len,n-done);size_t copied=k-copy_to_user(v.iov_base,(const char *)src+done,k);iov_iter_advance(i,copied);done+=copied;if(copied<k)break;}return done;}
static void iov_iter_init(struct iov_iter *i,int type,struct iovec *v,size_t nr,size_t n){*i=(struct iov_iter){v,nr,n,0};}
static void init_sync_kiocb(struct kiocb *i,struct file *f){*i=(struct kiocb){.ki_filp=f,.sync=true};}
static bool is_sync_kiocb(struct kiocb *i){return i->sync;}
static int kiocb_set_rw_flags(struct kiocb *i,unsigned flags){return flags?-EOPNOTSUPP:0;}
static ssize_t backend_read(struct file *f,char *buf,size_t count,loff_t *pos){if(native_error)return native_error;if(force_early_eof)return 0;if(*pos<0)return -EINVAL;if((size_t)*pos>=f->length)return 0;size_t wanted=min_t(size_t,count,f->length-*pos);if(f==&module_file)wanted=min_t(size_t,wanted,module_chunk);size_t copied=wanted-copy_to_user(buf,f->data+*pos,wanted);*pos+=copied;return copied?copied:(wanted?-EFAULT:0);}
static ssize_t backend_iter(struct kiocb *i,struct iov_iter *to){struct file *f=i->ki_filp;if(native_error)return native_error;if(force_early_eof)return 0;if(i->ki_pos<0)return -EINVAL;if((size_t)i->ki_pos>=f->length)return 0;size_t wanted=min_t(size_t,to->count,f->length-i->ki_pos);size_t copied=copy_to_iter(f->data+i->ki_pos,wanted,to);i->ki_pos+=copied;return copied?copied:(wanted?-EFAULT:0);}
static ssize_t call_read_iter(struct file *f,struct kiocb *i,struct iov_iter *to){return f->f_op->read_iter(i,to);}
static ssize_t call_write_iter(struct file *f,struct kiocb *i,struct iov_iter *to){return -EIO;}
static loff_t native_seek(struct file *f,loff_t off,int whence){if(whence==SEEK_END)off+=f->length;else if(whence==SEEK_CUR)off+=f->f_pos;if(off<0)return -EINVAL;f->f_pos=off;return off;}
static const struct file_operations read_ops={.read=backend_read,.llseek=native_seek},iter_ops={.read_iter=backend_iter,.llseek=native_seek};
static loff_t no_llseek(struct file *f,loff_t off,int whence){return -ESPIPE;}
static loff_t generic_file_llseek_size(struct file *f,loff_t off,int whence,loff_t max,loff_t eof){if(whence==SEEK_END)off+=eof;if(off<0||off>max)return -EINVAL;f->f_pos=off;return off;}
static struct inode *file_inode(struct file *f){return f->f_path.dentry->d_inode;}
static loff_t i_size_read(struct inode *i){return i->size;}
static int file_count(struct file *f){return f->refs;}
static char *d_path(const struct path *p,char *buf,size_t count){if(strlen(p->full)>=count)return ERR_PTR(-ENAMETOOLONG);strcpy(buf,p->full);return buf;}
static struct fd fdget_raw(unsigned fd){if(fd!=7||!fd_file)return (struct fd){NULL};fd_file->refs++;return (struct fd){fd_file};}
static void fdput(struct fd f){assert(f.file->refs>1);f.file->refs--;}
static struct file *fget(unsigned fd){return fdget_raw(fd).file;}
static void fput(struct file *f){fdput((struct fd){f});}
static int vfs_getattr(const struct path *p,struct kstat *s,u32 mask,unsigned flags){stat_calls++;if(getattr_error)return getattr_error;s->size=p->dentry->d_inode->size;s->other=123;s->result_mask=omit_size?0:STATX_SIZE;return 0;}
static const struct cred *override_creds(const struct cred *c){assert(c==ksu_cred && ready);const struct cred *old=subjective;subjective=c;override_count++;return old;}
static void revert_creds(const struct cred *c){subjective=c;revert_count++;}
static void *checked_alloc(size_t n,bool clear){int attempt=__atomic_add_fetch(&alloc_attempts,1,__ATOMIC_RELAXED);if(attempt==fail_alloc_at)return NULL;void *p=clear?calloc(1,n):malloc(n);assert(p);__atomic_add_fetch(&live_allocations,1,__ATOMIC_RELAXED);return p;}
static void *vmalloc(size_t n){return checked_alloc(n,false);}
static void *kzalloc(size_t n,int flags){return checked_alloc(n,true);}
static void vfree(void *p){if(p){free(p);__atomic_sub_fetch(&live_allocations,1,__ATOMIC_RELAXED);}}
static void kfree(void *p){vfree(p);}
static struct file *filp_open(const char *name,int flags,int mode){assert(subjective==ksu_cred && (flags&O_NONBLOCK));opens++;int error=strstr(name,"watchdog")?first_open_error:second_open_error;return error?ERR_PTR(error):&module_file;}
static int filp_close(struct file *f,void *owner){assert(f==&module_file);closes++;
#ifdef CONFIG_KSU_SUSFS
ksu_handle_initrc_file_release(f); /* Stronger than native deferred __fput. */
#endif
return 0;}
static ssize_t vfs_read(struct file *f,char *buf,size_t count,loff_t *pos);
static ssize_t ksu_kernel_read_compat(struct file *f,void *buf,size_t count,loff_t *pos){if(module_error)return module_error;return vfs_read(f,buf,count,pos);}
#ifdef CONFIG_KSU_SUSFS
bool ksu_susfs_cred_ready(void){return ready;}
#include "provider_init_rc.h"
#endif
#include "native_reads.h"
#include "native_fstat_seek.h"
static void make_file(struct file *f,int slot,const char *full,const struct file_operations *ops){inodes[slot]=(struct inode){S_IFREG,sizeof(base_data)-1,&sb};dentries[slot]=(struct dentry){&inodes[slot],{strrchr(full,'/')+1}};*f=(struct file){.f_path={&dentries[slot],full},.f_mode=FMODE_READ|FMODE_CAN_READ|FMODE_OPENED|FMODE_LSEEK,.f_op=ops,.private_data=(void *)0x1234,.data=base_data,.length=sizeof(base_data)-1,.refs=1};}
static void reset(void){
#ifdef CONFIG_KSU_SUSFS
while(!list_empty(&ksu_devon_rc_views)){struct ksu_devon_rc_view *v=container_of(ksu_devon_rc_views.next,struct ksu_devon_rc_view,node);ksu_handle_initrc_file_release(v->file);}vfree(ksu_devon_rc_tail);ksu_devon_rc_tail=NULL;ksu_devon_rc_tail_len=0;ksu_devon_rc_loaded=false;
#endif
assert(!live_allocations);subjective=&init_cred;task.pid=1;strcpy(task.comm,"init");ready=init_rc_enabled=true;ksu_no_custom_rc=deny_access=force_early_eof=omit_size=false;rw_error=getattr_error=native_error=opens=closes=override_count=revert_count=read_notifications=stat_calls=0;first_open_error=second_open_error=module_error=alloc_attempts=fail_alloc_at=0;module_chunk=2;copy_budget=SIZE_MAX;make_file(&rc[0],0,"/init.rc",&read_ops);make_file(&rc[1],1,"/system/etc/init/hw/init.rc",&iter_ops);make_file(&module_file,2,"/metadata/ksu/modules.rc",&read_ops);module_file.data=module_data;module_file.length=sizeof(module_data)-1;inodes[2].size=module_file.length;fd_file=&rc[0];}
static loff_t stat_size(void){struct kstat s={0};assert(!vfs_statx_fd(7,&s,0x7ff,0)&&s.other==123);assert(fd_file->refs==1);return s.size;}
static void unchanged(void){assert(rc[0].f_op==&read_ops&&rc[1].f_op==&iter_ops&&rc[0].private_data==(void *)0x1234&&rc[1].private_data==(void *)0x1234);assert(subjective==&init_cred&&override_count==revert_count);}
static void test_native_errors(void){char buf[128];loff_t pos=0;struct kstat s={.size=42};reset();omit_size=true;assert(vfs_statx_fd(7,&s,0,0)==0&&!alloc_attempts&&s.size==(loff_t)rc[0].length);s.size=42;reset();fd_file=NULL;assert(vfs_statx_fd(7,&s,0,0)==-EBADF&&s.size==42);assert(vfs_statx_fd(7,&s,0,0x80000000)==-EINVAL);reset();getattr_error=-EACCES;assert(vfs_statx_fd(7,&s,0,0)==-EACCES&&s.size==42&&!alloc_attempts);reset();rc[0].f_mode=0;assert(vfs_read(&rc[0],buf,1,&pos)==-EBADF&&!alloc_attempts);reset();rc[0].f_mode=FMODE_READ;assert(vfs_read(&rc[0],buf,1,&pos)==-EINVAL&&!alloc_attempts);reset();deny_access=true;assert(vfs_read(&rc[0],buf,1,&pos)==-EFAULT&&!alloc_attempts);reset();rw_error=-EACCES;assert(vfs_read(&rc[0],buf,1,&pos)==-EACCES&&!alloc_attempts);reset();assert(!vfs_read(&rc[0],NULL,0,&pos)&&!alloc_attempts);reset();rc[0].f_mode&=~FMODE_LSEEK;assert(vfs_llseek(&rc[0],0,SEEK_END)==-ESPIPE&&!alloc_attempts);unchanged();}
#ifdef CONFIG_KSU_SUSFS
static void test_views(void){char got[4096],expect[4096];reset();size_t total=rc[0].length+ksu_rc_len+module_file.length;memcpy(expect,base_data,rc[0].length);memcpy(expect+rc[0].length,KERNEL_SU_RC,ksu_rc_len);memcpy(expect+rc[0].length+ksu_rc_len,module_data,module_file.length);assert(stat_size()==(loff_t)total);size_t done=0;loff_t pos=0;while(done<total){ssize_t n=vfs_read(&rc[0],got+done,3,&pos);assert(n>0&&n<=3);done+=n;}assert(!memcmp(expect,got,total)&&pos==(loff_t)total);assert(!vfs_read(&rc[0],got,3,&pos)&&stat_size()==(loff_t)total);init_rc_enabled=false;assert(stat_size()==(loff_t)total);init_rc_enabled=true;rc[0].f_path.full="/data/renamed.rc";dentries[0].d_name.name="renamed.rc";assert(stat_size()==(loff_t)total);assert(vfs_llseek(&rc[0],0,SEEK_END)==(loff_t)total);assert(vfs_llseek(&rc[0],LLONG_MAX,SEEK_END)==-EOVERFLOW);assert(!vfs_llseek(&rc[0],0,SEEK_SET));pos=0;assert(vfs_read(&rc[0],got,total,&pos)==(ssize_t)rc[0].length);assert(vfs_read(&rc[0],got+pos,total-pos,&pos)==(ssize_t)(ksu_rc_len+module_file.length));assert(!memcmp(expect,got,total));fd_file=&rc[1];assert(stat_size()==(loff_t)total);char a[5],b[4096];struct iovec vec[]={{a,sizeof(a)},{b,sizeof(b)}};struct iov_iter iter;pos=rc[1].length;iov_iter_init(&iter,READ,vec,2,ksu_rc_len+module_file.length);assert(do_iter_read(&rc[1],&iter,&pos,0)==(ssize_t)(ksu_rc_len+module_file.length));assert(!memcmp(a,KERNEL_SU_RC,5)&&!memcmp(b,expect+rc[1].length+5,ksu_rc_len+module_file.length-5));assert(pos==(loff_t)total&&!iter.count);unchanged();ksu_handle_initrc_file_release(&rc[0]);assert(ksu_devon_rc_find(&rc[0])==NULL&&ksu_devon_rc_find(&rc[1])!=NULL);make_file(&rc[0],0,"/init.rc",&read_ops);fd_file=&rc[0];assert(stat_size()==(loff_t)total);unchanged();}
static void test_partial_iter(void){char buf[2048];reset();loff_t total=stat_size();loff_t pos=rc[0].length;copy_budget=2;assert(vfs_read(&rc[0],buf,10,&pos)==2&&pos==(loff_t)rc[0].length+2);copy_budget=0;assert(vfs_read(&rc[0],buf,10,&pos)==-EFAULT&&pos==(loff_t)rc[0].length+2);copy_budget=SIZE_MAX;ssize_t remaining=total-pos;assert(vfs_read(&rc[0],buf,sizeof(buf),&pos)==remaining&&pos==total);reset();fd_file=&rc[1];total=stat_size();struct iovec v={buf,sizeof(buf)};struct iov_iter it;loff_t p=0;iov_iter_init(&it,READ,&v,1,sizeof(buf));assert(do_iter_read(&rc[1],&it,&p,0)==(ssize_t)rc[1].length&&it.count==sizeof(buf)-rc[1].length);assert(do_iter_read(&rc[1],&it,&p,0)==total-(loff_t)rc[1].length);assert(!do_iter_read(&rc[1],&it,&p,0));reset();fd_file=&rc[1];total=stat_size();p=rc[1].length;v=(struct iovec){buf,sizeof(buf)};iov_iter_init(&it,READ,&v,1,10);copy_budget=2;assert(do_iter_read(&rc[1],&it,&p,0)==2&&it.count==8);copy_budget=0;assert(do_iter_read(&rc[1],&it,&p,0)==-EFAULT&&it.count==8);copy_budget=SIZE_MAX;struct kiocb io={&rc[1],rc[1].length,false};assert(vfs_iocb_iter_read(&rc[1],&io,&it)==-EOPNOTSUPP);io.sync=true;assert(vfs_iocb_iter_read(&rc[1],&io,&it)==8);iov_iter_init(&it,READ,&v,1,1);p=0;assert(do_iter_read(&rc[1],&it,&p,1)==-EOPNOTSUPP);unchanged();}
static void test_failures_gates(void){char buf[2048];struct kstat st;for(int failure=1;failure<=3;failure++){reset();fail_alloc_at=failure;assert(vfs_statx_fd(7,&st,0,0)==-ENOMEM&&!ksu_devon_rc_find(&rc[0]));fail_alloc_at=0;assert(stat_size()>(loff_t)rc[0].length);unchanged();}for(int kind=0;kind<4;kind++){reset();if(kind==0)first_open_error=second_open_error=-EIO;if(kind==1)module_error=-EIO;if(kind==2)inodes[2].size=SZ_1M+1;if(kind==3)inodes[2].i_mode=S_IFDIR;assert(vfs_statx_fd(7,&st,0,0)==(kind==2?-EFBIG:kind==3?-EINVAL:-EIO));assert(!ksu_devon_rc_loaded&&!ksu_devon_rc_find(&rc[0]));unchanged();}reset();first_open_error=second_open_error=-ENOENT;assert(stat_size()==(loff_t)(rc[0].length+ksu_rc_len));reset();first_open_error=-ENOTDIR;second_open_error=-ENOENT;assert(stat_size()==(loff_t)(rc[0].length+ksu_rc_len));reset();ksu_no_custom_rc=true;assert(stat_size()==(loff_t)(rc[0].length+ksu_rc_len)&&!opens);reset();assert(stat_size()>(loff_t)rc[0].length);inodes[0].size++;assert(vfs_statx_fd(7,&st,0,0)==-ESTALE);loff_t pos=0;assert(vfs_read(&rc[0],buf,1,&pos)==-ESTALE);reset();inodes[0].size=LLONG_MAX;assert(vfs_statx_fd(7,&st,0,0)==-EOVERFLOW);reset();assert(stat_size()>(loff_t)rc[0].length);force_early_eof=true;pos=0;assert(vfs_read(&rc[0],buf,1,&pos)==-EIO);reset();native_error=-EIO;pos=0;assert(vfs_read(&rc[0],buf,1,&pos)==-EIO);for(int gate=0;gate<8;gate++){reset();if(gate==0)ready=false;if(gate==1)subjective=&other_cred;if(gate==2)task.pid=2;if(gate==3)strcpy(task.comm,"other");if(gate==4)rc[0].f_mode|=FMODE_WRITE;if(gate==5)rc[0].f_flags=O_DIRECT;if(gate==6)rc[0].f_mode|=FMODE_STREAM;if(gate==7)init_rc_enabled=false;assert(stat_size()==(loff_t)rc[0].length&&!alloc_attempts&&!opens);subjective=&init_cred;unchanged();}reset();rc[0].refs=0;loff_t dead_size=rc[0].length;assert(!ksu_handle_vfs_fstat_file(&rc[0],&dead_size)&&!alloc_attempts);rc[0].refs=1;reset();rc[0].f_path.full="/data/init.rc";assert(stat_size()==(loff_t)rc[0].length&&!opens);unchanged();}
static void *first_view(void *p){subjective=&init_cred;loff_t n=rc[0].length;assert(!ksu_handle_vfs_fstat_file(&rc[0],&n)&&n>(loff_t)rc[0].length);return NULL;}
static void test_concurrent(void){reset();pthread_t t[8];for(int i=0;i<8;i++)assert(!pthread_create(&t[i],NULL,first_view,NULL));for(int i=0;i<8;i++)assert(!pthread_join(t[i],NULL));int count=0;struct ksu_devon_rc_view *v;list_for_each_entry(v,&ksu_devon_rc_views,node)count++;assert(count==1&&opens==1&&closes==1&&live_allocations==2);unchanged();}
#endif
int main(void){reset();test_native_errors();
#ifdef CONFIG_KSU_SUSFS
test_views();test_partial_iter();test_failures_gates();test_concurrent();
#endif
reset();puts("PASS: native read/fstat/seek error order; held-file logical sizes and positions, partial user/iterator copies, module failures/retries, per-file close/reuse and concurrent publication");return 0;}
