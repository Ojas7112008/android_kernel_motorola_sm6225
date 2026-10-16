// SPDX-License-Identifier: GPL-2.0
/* FUSE-private objects and IO are modeled; generated headers are actual source. */
#define main base_consumer_main
#include "test_path_consumers.c"
#undef main
#include <sched.h>

_Static_assert(sizeof(struct susfs_fuse_inode)==40, "64-bit private entry");
#define I_DIRTY_INODE 1UL
#define I_FREEING 2UL
#define I_CLEAR 4UL
#define SB_ACTIVE 1UL
#define FUSE_NAME_MAX 1024
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE-1))
#define FOPEN_CACHE_DIR 1
#define GFP_KERNEL 0
#define NSEC_PER_SEC 1000000000L
#define min_t(t,a,b) ((t)(a)<(t)(b)?(t)(a):(t)(b))
#undef WARN_ON
#define WARN_ON(x) ({ bool warned=!!(x); assert(!warned); warned; })
#ifdef CONFIG_FUSE_DAX
#define MODEL_DAX 1
#else
#define MODEL_DAX 0
#endif
#define IS_ENABLED(x) MODEL_DAX
struct fuse_forget_link { int value; };
struct fuse_inode {
 struct inode inode;
#ifdef CONFIG_FUSE_BPF
 struct inode *backing_inode;
 void *bpf;
#endif
 u64 i_time, nodeid, nlookup, attr_version, orig_ino;
 u32 inval_mask; unsigned long state; umode_t orig_i_mode;
 pthread_mutex_t mutex; int i_mmap_sem; spinlock_t lock;
 struct fuse_forget_link *forget; void *dax;
 struct list_head write_files, queued_writes;
 struct susfs_fuse_inode susfs_path;
};
struct fuse_attr { u64 ino,size,blocks,atime,mtime,ctime;u32 atimensec,mtimensec,ctimensec,mode,nlink,uid,gid,blksize,rdev; };
struct fuse_conn { u64 attr_version;bool writeback_cache,default_permissions;void *user_ns; };
static struct fuse_conn fc;
static void *fuse_inode_cachep;
static _Thread_local int fail_alloc_after=-1;
static _Thread_local bool fail_dax;
static int live_allocs, clears, bpf_puts, cache_adds, plus_links, plus_forgets;
static void *model_alloc(size_t n) {
 assert(model_spin_depth==0);
 if(fail_alloc_after==0)return NULL;
 if(fail_alloc_after>0)fail_alloc_after--;
 void *v=calloc(1,n);assert(v);__atomic_add_fetch(&live_allocs,1,__ATOMIC_RELAXED);return v;
}
static void model_free(void *v) { assert(model_spin_depth==0);if(v) {__atomic_sub_fetch(&live_allocs,1,__ATOMIC_RELAXED);free(v);} }
static void *kmem_cache_alloc(void *cache,int mode) { (void)cache;(void)mode;return model_alloc(sizeof(struct fuse_inode)); }
static void kmem_cache_free(void *cache,void *v) { (void)cache;model_free(v); }
#define kfree model_free
static struct fuse_forget_link *fuse_alloc_forget(void) { return model_alloc(sizeof(struct fuse_forget_link)); }
static void mutex_init(pthread_mutex_t *m) { assert(!pthread_mutex_init(m,NULL)); }
static void mutex_destroy(pthread_mutex_t *m) { assert(!pthread_mutex_destroy(m)); }
static void spin_lock_init(spinlock_t *m) { assert(!pthread_mutex_init(m,NULL)); }
static void init_rwsem(int *r) { *r=0; }
static void list_init(struct list_head *h) { h->next=h->prev=h; }
static bool list_empty(const struct list_head *h) { return h->next==h; }
static bool fuse_dax_inode_alloc(struct super_block *s,struct fuse_inode *fi) { (void)s;if(fail_dax)return false;fi->dax=model_alloc(8);return !!fi->dax; }
static void fuse_dax_inode_cleanup(struct inode *i) { (void)i; }
#define FUSE_IS_DAX(i) MODEL_DAX
static void bpf_prog_put(void *v) { assert(v);bpf_puts++; }
static void truncate_inode_pages_final(struct address_space *m) { (void)m; }
static struct fuse_inode *get_fuse_inode(struct inode *inode);
static void clear_inode(struct inode *i) {
 assert(hlist_unhashed(&get_fuse_inode(i)->susfs_path.node));
 __atomic_add_fetch(&clears,1,__ATOMIC_RELAXED);
 i->i_mapping=NULL;i->i_state=I_FREEING|I_CLEAR;
}
static struct fuse_conn *get_fuse_conn(struct inode *i) { (void)i;return &fc; }
static void fuse_queue_forget(struct fuse_conn *c,struct fuse_forget_link *p,u64 nodeid,u64 nlookup) { (void)c;(void)nodeid;(void)nlookup;model_free(p); }
static bool is_bad_inode(struct inode *i) { (void)i;return false; }
#define lockdep_assert_held(s) assert(model_spin_depth>0)
static u64 atomic64_inc_return(u64 *v) { return __atomic_add_fetch(v,1,__ATOMIC_RELAXED); }
static void set_nlink(struct inode *i,unsigned n) { i->i_nlink=n; }
static kuid_t make_kuid(void *ns,unsigned v) { (void)ns;return (kuid_t){v}; }
static kuid_t make_kgid(void *ns,unsigned v) { return make_kuid(ns,v); }
#include "fuse_lifecycle.h"

struct fuse_dirent { u64 ino,off;u32 namelen,type;char name[]; };
struct fuse_entry_out { u64 nodeid; };
struct fuse_direntplus { struct fuse_entry_out entry_out; struct fuse_dirent dirent; };
struct fuse_file { unsigned open_flags;struct { loff_t pos;unsigned cache_off; } readdir; };
#define FUSE_NAME_OFFSET offsetof(struct fuse_dirent,name)
#define FUSE_NAME_OFFSET_DIRENTPLUS offsetof(struct fuse_direntplus,dirent.name)
#define FUSE_DIRENT_SIZE(d) ALIGN(FUSE_NAME_OFFSET+(d)->namelen,8)
#define FUSE_DIRENTPLUS_SIZE(d) ALIGN(FUSE_NAME_OFFSET_DIRENTPLUS+(d)->dirent.namelen,8)
static void fuse_add_dirent_to_cache(struct file *f,struct fuse_dirent *d,loff_t pos) { (void)f;(void)d;(void)pos;cache_adds++; }
static int fuse_direntplus_link(struct file *f,struct fuse_direntplus *d,u64 version) { (void)f;(void)d;(void)version;plus_links++;return 0; }
static void fuse_force_forget(struct file *f,u64 id) { (void)f;(void)id;plus_forgets++; }
#include "fuse_parsers.h"

static struct fuse_attr attr_new(u64 ino,unsigned owner) {
 return (struct fuse_attr){.ino=ino,.mode=0644,.uid=owner,.gid=owner,.nlink=1,.blksize=4096};
}
static struct fuse_inode *object_new(struct super_block *super,u64 ino,unsigned owner,bool marked) {
 struct inode *i=fuse_alloc_inode(super);assert(i);
 struct fuse_inode *fi=get_fuse_inode(i);
 i->i_sb=super;i->i_mapping=&i->i_data;i->i_mode=S_IFREG;i->refs=1;
 list_init(&fi->write_files);list_init(&fi->queued_writes);
 struct fuse_attr a=attr_new(ino,owner);
 spin_lock(&fi->lock);fuse_change_attributes_common(i,&a,1);spin_unlock(&fi->lock);
 if(marked)__atomic_fetch_or(&i->i_mapping->flags,BIT(33),__ATOMIC_RELAXED);
 return fi;
}
static void update_object(struct fuse_inode *fi,u64 ino,unsigned owner) {
 struct fuse_attr a=attr_new(ino,owner);
 spin_lock(&fi->lock);fuse_change_attributes_common(&fi->inode,&a,2);spin_unlock(&fi->lock);
}
static void object_free(struct fuse_inode *fi,bool evict_first) {
 if(evict_first) { fi->inode.i_state=I_FREEING;fuse_evict_inode(&fi->inode); }
#ifdef CONFIG_FUSE_BPF
 fuse_destroy_inode(&fi->inode);
#endif
 fuse_free_inode(&fi->inode);
}
static void empty_index(void) { for(unsigned i=0;i<ARRAY_SIZE(susfs_fuse_inodes);i++)assert(!susfs_fuse_inodes[i].first); }
static void test_identity(void) {
 reset();fc=(struct fuse_conn){0};sb.s_magic=FUSE_SUPER_MAGIC;
 struct fuse_inode *fi=object_new(&sb,9,0,true);fi->nodeid=999999;
 assert(susfs_fuse_is_hidden(&sb,9));assert(!susfs_fuse_is_hidden(&sb,fi->nodeid));
 struct super_block other={.s_magic=FUSE_SUPER_MAGIC};assert(!susfs_fuse_is_hidden(&other,9));
 uid.val=0;assert(!susfs_fuse_is_hidden(&sb,9));uid.val=1000;assert(!susfs_fuse_is_hidden(&sb,9));
 uid.val=10001;thread_flags=0;assert(!susfs_fuse_is_hidden(&sb,9));thread_flags=BIT(33);
 update_object(fi,9,10001);assert(!susfs_fuse_is_hidden(&sb,9));
 uid.val=110001;assert(susfs_fuse_is_hidden(&sb,9));update_object(fi,9,110001);assert(!susfs_fuse_is_hidden(&sb,9));
 uid.val=10001;update_object(fi,1ULL<<40,0);assert(!susfs_fuse_is_hidden(&sb,9));assert(susfs_fuse_is_hidden(&sb,1ULL<<40));
 __atomic_fetch_and(&fi->inode.i_mapping->flags,~BIT(33),__ATOMIC_RELAXED);assert(!susfs_fuse_is_hidden(&sb,1ULL<<40));
 __atomic_fetch_or(&fi->inode.i_mapping->flags,BIT(33),__ATOMIC_RELAXED);
 struct fuse_inode *peer=object_new(&sb,1ULL<<40,0,false);assert(!susfs_fuse_is_hidden(&sb,1ULL<<40));
 __atomic_fetch_or(&peer->inode.i_mapping->flags,BIT(33),__ATOMIC_RELAXED);assert(susfs_fuse_is_hidden(&sb,1ULL<<40));
 update_object(peer,1ULL<<40,10001);assert(!susfs_fuse_is_hidden(&sb,1ULL<<40));
 object_free(peer,true);assert(susfs_fuse_is_hidden(&sb,1ULL<<40));
 object_free(fi,true);assert(!susfs_fuse_is_hidden(&sb,1ULL<<40));empty_index();assert(live_allocs==0);
}
static void test_backing_identity(void) {
#ifdef CONFIG_FUSE_BPF
 reset();sb.s_magic=FUSE_SUPER_MAGIC;
 struct super_block backing_sb={.s_magic=0xef53};
 visible.i_sb=&backing_sb;visible.i_ino=431;visible.i_mode=S_IFREG|0644;
 visible.i_uid=(kuid_t){0};visible.i_gid=(kuid_t){0};visible.i_blkbits=12;
 struct fuse_attr backing_attr;fuse_fill_attr_from_inode(&backing_attr,&visible);
 struct fuse_inode *fi=object_new(&sb,9000,0,true);fi->nodeid=0;fi->backing_inode=&visible;visible.refs++;
 spin_lock(&fi->lock);fuse_change_attributes_common(&fi->inode,&backing_attr,2);spin_unlock(&fi->lock);
 assert(susfs_fuse_is_hidden(&sb,431));assert(!susfs_fuse_is_hidden(&backing_sb,431));assert(!susfs_fuse_is_hidden(&sb,9000));
 object_free(fi,true);assert(visible.refs==1 && live_allocs==0);empty_index();
#endif
}
static void test_lifetime(void) {
 reset();sb.s_magic=FUSE_SUPER_MAGIC;
 for(int f=0;f<2+MODEL_DAX;f++) { fail_alloc_after=f;assert(!fuse_alloc_inode(&sb));assert(live_allocs==0);empty_index(); }
 fail_alloc_after=-1;
 if(MODEL_DAX) { fail_dax=true;assert(!fuse_alloc_inode(&sb));fail_dax=false;assert(live_allocs==0);empty_index(); }
 struct inode *i=fuse_alloc_inode(&sb);assert(i);fuse_free_inode(i);assert(live_allocs==0);empty_index();
 struct fuse_inode *fi=object_new(&sb,42,0,true);
#ifdef CONFIG_FUSE_BPF
 fi->backing_inode=&visible;visible.refs++;fi->bpf=(void *)1;
#endif
 sb.s_flags=SB_ACTIVE;fi->nlookup=3;
 object_free(fi,true);assert(!susfs_fuse_is_hidden(&sb,42));assert(live_allocs==0 && visible.refs==1);empty_index();
 /* Direct destruction must unlink even if the eviction callback was skipped. */
 fi=object_new(&sb,43,0,true);object_free(fi,false);assert(!susfs_fuse_is_hidden(&sb,43));assert(live_allocs==0);empty_index();
 fi=object_new(&sb,42,10001,false);assert(!susfs_fuse_is_hidden(&sb,42));object_free(fi,true);empty_index();
}
static size_t add_record(unsigned char *buf,size_t pos,const char *name,u64 ino,u64 off,bool plus) {
 struct fuse_dirent *d;
 if(plus) { struct fuse_direntplus *dp=(void *)(buf+pos);dp->entry_out.nodeid=100+ino;d=&dp->dirent; }
 else d=(void *)(buf+pos);
 d->ino=ino;d->off=off;d->namelen=strlen(name);d->type=8;memcpy(d->name,name,d->namelen);
 return pos+(plus?ALIGN(FUSE_NAME_OFFSET_DIRENTPLUS+d->namelen,8):FUSE_DIRENT_SIZE(d));
}
static unsigned char stream[PAGE_SIZE] __attribute__((aligned(16)));
static size_t stream_size;
static int parser_route;
static struct fuse_file ff;
static int stream_iterate(struct file *f,struct dir_context *ctx) {
 switch(parser_route) {
 case 0:return parse_dirfile((void *)stream,stream_size,f,ctx);
 case 1:return parse_dirplusfile((void *)stream,stream_size,f,ctx,1);
 case 2:return fuse_parse_cache(&ff,stream,stream_size,ctx)==FOUND_ERR?-EIO:0;
 default:return parse_backing_dirfile((void *)stream,stream_size,ctx);
 }
}
static const struct file_operations stream_ops={.iterate_shared=stream_iterate};
static void prepare_stream(int route) {
 setup_entries();parser_route=route;cache_miss=true;memset(stream,0,sizeof(stream));
 stream_size=add_record(stream,0,"secret",9,101,route==1);
 stream_size=add_record(stream,stream_size,"public",10,205,route==1);
 ff=(struct fuse_file){.open_flags=FOPEN_CACHE_DIR};
 dirfile.f_op=&stream_ops;dirfile.private_data=&ff;cache_adds=plus_links=plus_forgets=0;
}
static void test_streams(void) {
 _Alignas(16) unsigned char out[512];
 for(int route=0;route<4;route++) {
  reset();sb.s_magic=FUSE_SUPER_MAGIC;struct fuse_inode *fi=object_new(&sb,9,0,true);fi->nodeid=6000;prepare_stream(route);
  long r=ksys_getdents64(3,(void *)out,sizeof(out));assert(r>0);
  struct linux_dirent64 *d=(void *)out;assert(!strcmp(d->d_name,"public") && r==d->d_reclen && d->d_off==205 && dirfile.f_pos==205);
  assert(fd_refs==0 && fi->inode.refs==1 && mount.refs==1 && parent.refs==1);
  if(route<2)assert(cache_adds==2);if(route==1)assert(plus_links==2 && plus_forgets==0);
  if(route==2)assert(ff.readdir.cache_off==stream_size && ff.readdir.pos==205);
  /* Both records hidden: zero output, final cookie still advances. */
  struct fuse_inode *second=object_new(&sb,10,0,true);prepare_stream(route);memset(out,0xaa,sizeof(out));
  r=ksys_getdents64(3,(void *)out,1);assert(r==0 && dirfile.f_pos==205);for(unsigned n=0;n<sizeof(out);n++)assert(out[n]==0xaa);
  object_free(second,true);object_free(fi,true);assert(live_allocs==0);empty_index();
 }
 /* Every native/compat emitter consumes the same cache-independent lookup. */
 reset();sb.s_magic=FUSE_SUPER_MAGIC;struct fuse_inode *fi=object_new(&sb,9,0,true);prepare_stream(0);
 assert(sys_old_readdir(3,(void *)out,sizeof(out))==1);assert(!strcmp(((struct old_linux_dirent *)out)->d_name,"public"));
 prepare_stream(0);assert(sys_getdents(3,(void *)out,sizeof(out))>0);assert(!strcmp(((struct linux_dirent *)out)->d_name,"public"));
 prepare_stream(0);assert(compat_sys_old_readdir(3,(void *)out,sizeof(out))==1);assert(!strcmp(((struct compat_old_linux_dirent *)out)->d_name,"public"));
 prepare_stream(0);assert(compat_sys_getdents(3,(void *)out,sizeof(out))>0);assert(!strcmp(((struct compat_linux_dirent *)out)->d_name,"public"));
 object_free(fi,true);assert(live_allocs==0);empty_index();
}
static int stop_readers;
static void *index_reader(void *arg) {
 (void)arg;while(!__atomic_load_n(&stop_readers,__ATOMIC_ACQUIRE)) {
  for(u64 ino=9;ino<13;ino++)(void)susfs_fuse_is_hidden(&sb,ino);
  sched_yield();
 }return NULL;
}
static void *index_writer(void *arg) {
 unsigned n=(uintptr_t)arg;
 for(int i=0;i<500;i++) {
  struct fuse_inode *fi=object_new(&sb,9+n,0,true);update_object(fi,9+n,10001);update_object(fi,9+n,0);
  if(i&1)susfs_fuse_inode_forget(&fi->susfs_path);
  object_free(fi,true);
 }return NULL;
}
static void test_concurrency(void) {
 reset();sb.s_magic=FUSE_SUPER_MAGIC;stop_readers=0;pthread_t readers[4],writers[4];
 for(unsigned n=0;n<4;n++)assert(!pthread_create(&readers[n],NULL,index_reader,NULL));
 for(unsigned n=0;n<4;n++)assert(!pthread_create(&writers[n],NULL,index_writer,(void *)(uintptr_t)n));
 for(unsigned n=0;n<4;n++)assert(!pthread_join(writers[n],NULL));
 __atomic_store_n(&stop_readers,1,__ATOMIC_RELEASE);
 for(unsigned n=0;n<4;n++)assert(!pthread_join(readers[n],NULL));
 empty_index();assert(live_allocs==0 && model_spin_depth==0);
}
int main(void) {
 base_consumer_main();empty_index();test_identity();test_backing_identity();test_lifetime();test_streams();test_concurrency();
 puts("PASS: actual FUSE allocation/attributes/eviction/free and four directory parsers; cache-independent identity, ownership, ambiguous peers, faults, no retained references and concurrent teardown");return 0;
}
