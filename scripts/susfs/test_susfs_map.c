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
#define PAGE_SIZE 4096UL
#define PAGE_SHIFT 12
#define min(a,b) ((a) < (b) ? (a) : (b))
#define min_t(t,a,b) min((t)(a),(t)(b))
#define FOLL_WRITE 1
#define LOOKUP_FOLLOW 1
#define VM_SOFTDIRTY 2
#define PM_SOFT_DIRTY (1ULL << 55)
#define PM_PFRAME_MASK ((1ULL << 55) - 1)
#define PM_END_OF_BUFFER 1
#define GFP_KERNEL 0
#define PTRACE_MODE_READ_FSCREDS 1
#define BUG() abort()
#define IS_ERR_OR_NULL(p) (!(p))
typedef uint64_t u64;
typedef unsigned int fmode_t;
static unsigned long task_flags;
static unsigned int model_uid;
struct kuid_model { unsigned int val; };
static struct kuid_model current_uid(void) { return (struct kuid_model){model_uid}; }
#define test_thread_flag(n) (!!(task_flags & (1UL << (n))))
static bool test_bit(int n, const unsigned long *flags) { return !!(*flags & (1UL << n)); }
static void set_bit(int n, unsigned long *flags) { *flags |= 1UL << n; }
struct address_space { unsigned long flags; };
struct inode { struct address_space *i_mapping; };
struct dentry { struct inode *inode; };
struct path { struct dentry *dentry; };
struct file { struct inode *f_inode; struct path f_path; fmode_t f_mode; };
static struct inode *file_inode(const struct file *f) { return f->f_inode; }
static struct inode *d_backing_inode(struct dentry *d) { return d->inode; }
struct vm_area_struct;
struct vm_operations_struct { int (*access)(struct vm_area_struct *, unsigned long, void *, int, int); };
struct vm_area_struct { unsigned long vm_start, vm_end, vm_flags; struct vm_area_struct *vm_next; struct file *vm_file; struct vm_operations_struct *vm_ops; };
struct mm_struct { struct vm_area_struct *mmap; int locked, refs; };
struct task_struct { struct mm_struct *mm; int refs; };
struct page { unsigned char data[PAGE_SIZE]; };
static struct page pages[16];
static bool copy_fault, permission = true, lock_fail, gup_fail;
static int lookup_error, lookup_refs, lookup_calls, gup_calls, io_calls;
static struct dentry lookup_dentry;
static size_t copy_from_user(void *d, const void *s, size_t n)
{ if (copy_fault || !s) return n; memcpy(d,s,n); return 0; }
static size_t copy_to_user(void *d, const void *s, size_t n)
{ if (!d) return n; memcpy(d,s,n); return 0; }
static int kern_path(const char *name, int flags, struct path *p)
{ assert(name[0] && strlen(name) < 256 && flags == LOOKUP_FOLLOW); lookup_calls++; if (lookup_error) return lookup_error; p->dentry=&lookup_dentry; lookup_refs++; return 0; }
static void path_put(struct path *p) { assert(p->dentry && lookup_refs > 0); lookup_refs--; }
static int mmap_read_lock_killable(struct mm_struct *mm) { if (lock_fail) return -EINTR; mm->locked++; return 0; }
static void mmap_read_unlock(struct mm_struct *mm) { assert(mm->locked == 1); mm->locked--; }
static struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{ assert(mm->locked); struct vm_area_struct *v=mm->mmap; while(v && v->vm_end <= addr) v=v->vm_next; return v; }
static int get_user_pages_remote(struct task_struct *task, struct mm_struct *mm, unsigned long addr, int nr, unsigned int flags, struct page **page, struct vm_area_struct **vma, void *locked)
{ gup_calls++; *vma=find_vma(mm,addr); if (gup_fail || !*vma || (*vma)->vm_start > addr) return 0; *page=&pages[addr/PAGE_SIZE]; return 1; }
static void *kmap(struct page *page) { return page->data; }
static void kunmap(struct page *page) {}
static void put_page(struct page *page) {}
static void set_page_dirty_lock(struct page *page) {}
static void copy_from_user_page(struct vm_area_struct *v, struct page *p, unsigned long a, void *dst, const void *src, int n) { memcpy(dst,src,n); }
static void copy_to_user_page(struct vm_area_struct *v, struct page *p, unsigned long a, void *dst, const void *src, int n) { memcpy(dst,src,n); }
typedef struct { u64 pme; } pagemap_entry_t;
struct pagemapread { int pos, len; pagemap_entry_t *buffer; bool show_pfn; };
struct mm_walk { struct mm_struct *mm; void *private; };
struct proc_maps_private { struct inode *inode; struct task_struct *task; struct mm_struct *mm; };
struct seq_file { void *private; unsigned long first, last, total; };
struct mem_size_stats { unsigned long pages; };
static struct task_struct model_task;
static struct task_struct *get_proc_task(struct inode *inode) { model_task.refs++; return &model_task; }
static struct mm_struct *get_task_mm(struct task_struct *task) { task->mm->refs++; return task->mm; }
static bool mmget_not_zero(struct mm_struct *mm) { mm->refs++; return true; }
static void mmput(struct mm_struct *mm) { assert(mm->refs > 0); mm->refs--; }
static void put_task_struct(struct task_struct *task) { assert(task->refs > 0); task->refs--; }
static void hold_task_mempolicy(struct proc_maps_private *p) {}
static void release_task_mempolicy(struct proc_maps_private *p) {}
static void smap_gather_stats(struct vm_area_struct *v, struct mem_size_stats *s) { s->pages += (v->vm_end-v->vm_start)/PAGE_SIZE; }
static void show_vma_header_prefix(struct seq_file *m, unsigned long first, unsigned long last, int a,int b,int c,int d) { m->first=first; m->last=last; }
static void seq_pad(struct seq_file *m, char c) {}
static void seq_puts(struct seq_file *m, const char *s) {}
static void __show_smap(struct seq_file *m, struct mem_size_stats *s) { m->total=s->pages; }
struct map_files_info { unsigned long start,end; fmode_t mode; };
struct dir_context { unsigned long pos; unsigned long emitted[16]; size_t count; };
struct flex_array { struct map_files_info *entries; size_t count; };
static struct vm_area_struct *mark_between_passes;
static struct flex_array *flex_array_alloc(size_t size, size_t n,int flags)
{ struct flex_array *fa=calloc(1,sizeof(*fa)); fa->entries=calloc(n,sizeof(*fa->entries)); fa->count=n; return fa; }
static int flex_array_prealloc(struct flex_array *fa,int first,size_t n,int flags);
static int flex_array_put(struct flex_array *fa,size_t i,struct map_files_info *info,int flags)
{ assert(i<fa->count); fa->entries[i]=*info; return 0; }
static void *flex_array_get(struct flex_array *fa,size_t i) { assert(i<fa->count); return &fa->entries[i]; }
static void flex_array_free(struct flex_array *fa) { free(fa->entries); free(fa); }
static bool ptrace_may_access(struct task_struct *t,int mode) { return permission; }
static bool dir_emit_dots(struct file *f,struct dir_context *ctx) { if(ctx->pos<2) ctx->pos=2; return true; }
static void proc_map_files_instantiate(void) {}
static bool proc_fill_cache(struct file *f,struct dir_context *ctx,const char *name,unsigned len,void (*instantiate)(void),struct task_struct *task,void *mode)
{ unsigned long start,end; assert(sscanf(name,"%lx-%lx",&start,&end)==2 && end>start); ctx->emitted[ctx->count++]=start; return true; }

#include "linux/susfs.h"
#include "linux/susfs_map.h"
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
#include "map_handler.h"
#endif
#include "native_pagemap.h"
#include "native_rollup.h"
#include "native_remote.h"
#include "native_map_files.h"
static int flex_array_prealloc(struct flex_array *fa,int first,size_t n,int flags)
{ if(mark_between_passes) set_bit(AS_FLAGS_SUS_MAP,&file_inode(mark_between_passes->vm_file)->i_mapping->flags); return 0; }

static void test_mark(struct inode *inode)
{
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	struct st_susfs_sus_map info={ .target_pathname="/target", .err=99 };
	void *p=&info; lookup_dentry.inode=inode;
	susfs_add_sus_map(&p); assert(!info.err && test_bit(AS_FLAGS_SUS_MAP,&inode->i_mapping->flags) && !lookup_refs);
	int calls=lookup_calls;
	memset(info.target_pathname,'x',sizeof(info.target_pathname)); susfs_add_sus_map(&p); assert(info.err==-EINVAL && calls==lookup_calls);
	info.target_pathname[0]=0; susfs_add_sus_map(&p); assert(info.err==-EINVAL && calls==lookup_calls);
	strcpy(info.target_pathname,"/target"); lookup_error=-EACCES; susfs_add_sus_map(&p); assert(info.err==-EACCES && !lookup_refs); lookup_error=0;
	copy_fault=true; susfs_add_sus_map(&p); assert(info.err==-EFAULT); copy_fault=false;
	lookup_dentry.inode=NULL; susfs_add_sus_map(&p); assert(info.err==-ENOENT && !lookup_refs);
	p=NULL; susfs_add_sus_map(&p); susfs_add_sus_map(NULL);
#else
	set_bit(AS_FLAGS_SUS_MAP,&inode->i_mapping->flags);
#endif
}
static void test_pagemap(struct mm_struct *mm)
{
	pagemap_entry_t buffer[8]; memset(buffer,0xff,sizeof(buffer));
	struct pagemapread pm={.buffer=buffer,.len=8}; struct mm_walk walk={.mm=mm,.private=&pm};
	assert(!mmap_read_lock_killable(mm));
	assert(!pagemap_pte_hole(PAGE_SIZE,5*PAGE_SIZE,&walk));
	assert(pm.pos==4);
	assert(buffer[0].pme==PM_SOFT_DIRTY && buffer[2].pme==PM_SOFT_DIRTY);
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	assert(!buffer[1].pme);
	pm.pos=0; pm.len=2; assert(susfs_pagemap_hidden_range(0,4*PAGE_SIZE,&pm)==PM_END_OF_BUFFER);
	assert(pm.pos==2 && !buffer[0].pme && !buffer[1].pme);
#else
	assert(buffer[1].pme==PM_SOFT_DIRTY);
#endif
	assert(!buffer[3].pme); mmap_read_unlock(mm);
}
static int io_access(struct vm_area_struct *v,unsigned long addr,void *buf,int len,int write)
{ io_calls++; memset(buf,'I',len); return len; }
static void test_remote(struct mm_struct *mm)
{
	unsigned char buffer[PAGE_SIZE*3]; memset(buffer,0,sizeof(buffer));
	gup_calls=0;
	int n=__access_remote_vm(NULL,mm,PAGE_SIZE,buffer,2*PAGE_SIZE,0);
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	assert(n==PAGE_SIZE && gup_calls==1);
	assert(!__access_remote_vm(NULL,mm,2*PAGE_SIZE,buffer,32,FOLL_WRITE));
#else
	assert(n==2*PAGE_SIZE && gup_calls==2);
#endif
	assert(!mm->locked);
	/* A hole before the next hidden VMA must follow the native GUP failure. */
	gup_calls=0; assert(!__access_remote_vm(NULL,mm,0,buffer,32,0) && gup_calls==1);
#ifdef CONFIG_HAVE_IOREMAP_PROT
	struct vm_operations_struct ops={.access=io_access}; mm->mmap->vm_ops=&ops; gup_fail=true;
	n=__access_remote_vm(NULL,mm,PAGE_SIZE,buffer,2*PAGE_SIZE,0);
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	assert(n==PAGE_SIZE);
#else
	assert(n==2*PAGE_SIZE);
#endif
	assert(io_calls==1 && !mm->locked); gup_fail=false; mm->mmap->vm_ops=NULL;
#endif
	lock_fail=true; assert(!__access_remote_vm(NULL,mm,PAGE_SIZE,buffer,32,0)); lock_fail=false;
}
static void test_rollup(struct mm_struct *mm)
{
	struct proc_maps_private priv={.mm=mm}; struct seq_file m={.private=&priv};
	assert(!show_smaps_rollup(&m,NULL));
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	assert(m.total==2);
#else
	assert(m.total==3);
#endif
	assert(m.first==PAGE_SIZE && m.last==4*PAGE_SIZE && !mm->locked && !mm->refs && !model_task.refs);
}
static void test_map_files(struct mm_struct *mm,struct file *file)
{
	struct dir_context ctx={0}; assert(!proc_map_files_readdir(file,&ctx));
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	assert(ctx.count==2 && ctx.emitted[0]==PAGE_SIZE && ctx.emitted[1]==3*PAGE_SIZE);
	/* Mark another live mapping between count and fill: no uninitialized entry. */
	mark_between_passes=mm->mmap->vm_next->vm_next; ctx=(struct dir_context){0};
	assert(!proc_map_files_readdir(file,&ctx) && ctx.count==1); mark_between_passes=NULL;
#else
	assert(ctx.count==3);
#endif
	permission=false; ctx=(struct dir_context){0}; assert(proc_map_files_readdir(file,&ctx)==-EACCES && !ctx.count); permission=true;
	assert(!mm->locked && !mm->refs && !model_task.refs);
}
int main(void)
{
	struct address_space maps[3]={0}; struct inode inodes[3]; struct file files[3]; struct vm_area_struct vmas[3];
	memset(files,0,sizeof(files)); memset(vmas,0,sizeof(vmas));
	for(int i=0;i<3;i++) { inodes[i].i_mapping=&maps[i]; files[i].f_inode=&inodes[i]; vmas[i]=(struct vm_area_struct){.vm_start=(i+1)*PAGE_SIZE,.vm_end=(i+2)*PAGE_SIZE,.vm_flags=VM_SOFTDIRTY,.vm_file=&files[i],.vm_next=i<2?&vmas[i+1]:NULL}; }
	struct mm_struct mm={.mmap=vmas}; model_task.mm=&mm; test_mark(&inodes[1]);
	model_uid=10000; task_flags=1UL<<TIF_PROC_UMOUNTED;
	assert(susfs_is_hidden_vma(&vmas[1]) && !susfs_is_hidden_vma(&vmas[0]) && !susfs_is_hidden_vma(NULL));
	model_uid=0; assert(!susfs_is_hidden_vma(&vmas[1])); model_uid=10000;
	task_flags=0; assert(!susfs_is_hidden_vma(&vmas[1])); task_flags=1UL<<TIF_PROC_UMOUNTED;
	test_pagemap(&mm); test_remote(&mm); test_rollup(&mm); test_map_files(&mm,&files[0]);
	puts("map bounds/mark lifetime, mixed pagemap positions, rollup, remote read/write boundaries and two-pass map_files passed");
}
