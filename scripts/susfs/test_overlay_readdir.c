// SPDX-License-Identifier: GPL-2.0
/* Native iterator/helper bodies; filesystem, cache IO and locks are models. */
static unsigned last_lookup_flags, last_reval_flags;
static int backing_actor_depth;
#define main previous_consumer_main
#include "consumer_fixture.h"
#undef main
#define WARN_ON_ONCE(x) WARN_ON(x)
#ifndef EXPORT_SYMBOL_GPL
#define EXPORT_SYMBOL_GPL(x)
#endif
#ifndef WRITE_ONCE
#define WRITE_ONCE(p,v) ((p)=(v))
#endif
#define LIST_POISON1 NULL
#define LIST_POISON2 NULL
#define __list_add_valid(n,p,q) true
#define __list_del_entry_valid(e) true
#define pr_warn_ratelimited(...) ((void)0)
#include <limits.h>
#ifndef NAME_MAX
#define NAME_MAX 255
#endif
#define check_add_overflow(a,b,p) __builtin_add_overflow(a,b,p)
#define kfree(p) free(p)
static bool inode_is_locked(struct inode *i) { return i->i_rwsem>0; }
#include "native_metadata_lookup.h"
#include "native_overlay_lists.h"
struct rb_node { int unused; };
struct rb_root { struct rb_node *rb_node; };
#define RB_ROOT ((struct rb_root){NULL})
#include "native_overlay_dir_structs.h"
#include "native_overlay_dir_constants.h"
#include "native_dir_emit.h"
struct kstat { unsigned mode, dev; u64 ino; };
struct ovl_layer { int fsid; };
static struct ovl_layer layer={3};
static bool same_sb=true, origin=true, upper_type, impure, model_real;
static int backing_error, seek_error;
static int xino, getattr_error, cache_error, read_error, gets, direct_calls, resets, cache_gets;
static struct ovl_dir_cache cache;
static struct ovl_dir_file od;
static struct file overlay_file, real_file;
static struct inode backing_directory;
static struct dentry *overlay_model_lookup(struct inode *i,struct dentry *d,unsigned flags)
{
 struct inode *saved=resolved_inode;
 if(d->d_name.len==6 && !memcmp(d->d_name.name,"public",6))resolved_inode=&visible;
 struct dentry *result=model_lookup(i,d,flags);resolved_inode=saved;return result;
}
static const struct inode_operations overlay_iops={.lookup=overlay_model_lookup};
static int ovl_xino_bits(struct super_block *s) { assert(s==&sb);return xino; }
static bool ovl_same_sb(struct super_block *s) { assert(s==&sb);return same_sb; }
static enum ovl_path_type ovl_path_type(struct dentry *d) { (void)d;return (origin?__OVL_PATH_ORIGIN:0)|(upper_type?__OVL_PATH_UPPER:0); }
static struct ovl_layer *ovl_layer_lower(struct dentry *d) { (void)d;return &layer; }
static int vfs_getattr(struct path *p,struct kstat *stat,unsigned request,unsigned flags)
{
 assert(p->mnt==&mount && request==STATX_INO && flags==0);gets++;
 if(getattr_error)return getattr_error;
 *stat=(struct kstat){.mode=p->dentry->d_inode->i_mode,.dev=sb.s_dev,.ino=p->dentry->d_inode->i_ino+10000};return 0;
}
static bool ovl_cache_entry_find_link(const char *name,int len,struct rb_node ***link,struct rb_node **parent_node)
{ (void)name;(void)len;*parent_node=NULL;return **link!=NULL; }
static void rb_link_node(struct rb_node *node,struct rb_node *parent_node,struct rb_node **link)
{ (void)parent_node;*link=node; }
static void rb_insert_color(struct rb_node *node,struct rb_root *root) { assert(root->rb_node==node); }
static void ovl_path_upper(struct dentry *d,struct path *path) { assert(d==&parent);*path=(struct path){&parent,&mount}; }
static int ovl_fill_plain(struct dir_context *ctx,const char *name,int len,loff_t off,u64 ino,unsigned type)
{ (void)ctx;(void)name;(void)len;(void)off;(void)ino;(void)type;assert(false);return 0; }
static struct ovl_cache_entry *new_entry(const char *name,u64 ino)
{
 size_t len=strlen(name);struct ovl_cache_entry *p=calloc(1,sizeof(*p)+len+1);assert(p);
 p->len=len;p->type=8;p->real_ino=ino;p->ino=ino;memcpy(p->name,name,len+1);return p;
}
static int ovl_dir_read(struct path *path,struct ovl_readdir_data *rdd)
{
 assert(path->dentry==&parent);if(read_error)return read_error;
 struct ovl_cache_entry *p=new_entry("secret",22);list_add_tail(&p->l_node,rdd->list);return 0;
}
static u64 ovl_dentry_version_get(struct dentry *d) { return 0; }
static void ovl_cache_put(struct ovl_dir_file *dir,struct dentry *d) { }
static bool ovl_dir_is_real(struct dentry *d) { return model_real; }
static struct ovl_dir_cache *ovl_cache_get(struct dentry *d)
{ assert(d==&parent);cache_gets++;return cache_error?ERR_PTR(cache_error):&cache; }
static bool ovl_is_impure_dir(struct file *file) { assert(file==&overlay_file);return impure; }
static struct ovl_dir_cache *ovl_cache_get_impure(struct path *path)
{ return cache_error?ERR_PTR(cache_error):&cache; }
static struct ovl_cache_entry *ovl_cache_entry_find(struct rb_root *root,const char *name,int len)
{
 struct list_head *node;
 for(node=cache.entries.next;node!=&cache.entries;node=node->next) {
  struct ovl_cache_entry *p=list_entry(node,struct ovl_cache_entry,l_node);
  if(p->len==len && !memcmp(p->name,name,len))return p;
 }
 return NULL;
}
static void inode_lock(struct inode *inode) { inode->i_rwsem++; }
static loff_t vfs_llseek(struct file *file,loff_t offset,int origin_arg)
{
 assert(file==&real_file);
 if(seek_error)return seek_error;
 if(origin_arg==SEEK_CUR) { if(check_add_overflow(file->f_pos,offset,&offset))return -EINVAL; }
 else if(origin_arg!=SEEK_SET)return -EINVAL;
 if(offset<0)return -EINVAL;
 file->f_pos=offset;return offset;
}
static int backing_iterate(struct file *file,struct dir_context *ctx)
{
 const char *names[]={"secret","public"};
 assert(file==&real_file && backing_directory.i_rwsem>0);direct_calls++;backing_actor_depth++;
 for(unsigned i=0;i<2;i++) {
  if(ctx->pos>=(loff_t)((i+1)*10))continue;
  if(ctx->actor(ctx,names[i],strlen(names[i]),ctx->pos,22+i,8))break;
  ctx->pos=(i+1)*10;
  if(backing_error) { backing_actor_depth--;return backing_error; }
 }
 backing_actor_depth--;return 0;
}
static const struct file_operations backing_fops={.iterate_shared=backing_iterate};
#include "native_overlay_readdir.h"
static const struct file_operations overlay_fops={.iterate=ovl_iterate};

struct recording { struct dir_context ctx; unsigned count, limit; char names[8][24];u64 inos[8];loff_t cookies[8]; };
static int record(struct dir_context *ctx,const char *name,int len,loff_t pos,u64 ino,unsigned type)
{
 struct recording *r=container_of(ctx,struct recording,ctx);(void)type;
 if(r->count==r->limit)return -EINVAL;
 assert(len<24 && r->count<8);memcpy(r->names[r->count],name,len);r->names[r->count][len]=0;
 r->inos[r->count]=ino;r->cookies[r->count]=pos;r->count++;return 0;
}
static void begin(void)
{
 reset();sb.s_magic=OVERLAYFS_SUPER_MAGIC;directory.i_rwsem=1;visible.i_uid.val=0;
 parent.d_parent=&parent;directory.i_op=&overlay_iops;hidden.i_ino=71;visible.i_ino=72;
 same_sb=origin=true;upper_type=impure=false;xino=getattr_error=cache_error=read_error=0;
 model_real=false;backing_error=seek_error=backing_actor_depth=0;
 gets=direct_calls=resets=cache_gets=0;last_lookup_flags=last_reval_flags=~0U;
 cache=(struct ovl_dir_cache){.refcount=1};INIT_LIST_HEAD(&cache.entries);
 od=(struct ovl_dir_file){.cache=&cache,.cursor=&cache.entries,.realfile=&real_file};
 overlay_file=(struct file){.f_path={&parent,&mount},.f_inode=&directory,.private_data=&od,.f_op=&overlay_fops};
 backing_directory=directory;backing_directory.i_rwsem=0;
 real_file=(struct file){.f_path={&parent,&mount},.f_inode=&backing_directory,.f_op=&backing_fops};
}
static void end(void)
{
 while(cache.entries.next!=&cache.entries){struct ovl_cache_entry *p=list_entry(cache.entries.next,struct ovl_cache_entry,l_node);list_del(&p->l_node);free(p);}
 directory.i_rwsem=0;assert_refs();
}
static struct recording recorder(unsigned limit) { return (struct recording){.ctx={.actor=record},.limit=limit}; }
static void add_entry(const char *name,u64 ino,bool whiteout)
{ struct ovl_cache_entry *p=new_entry(name,ino);p->is_whiteout=whiteout;list_add_tail(&p->l_node,&cache.entries);od.cursor=cache.entries.next; }

static void test_metadata_lookup(void)
{
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
 for(unsigned cold=0;cold<3;cold++) {
  begin();cache_miss=cold!=0;lookup_alias=cold==2;
  struct dentry *d=susfs_lookup_one_len_metadata("secret",&parent,6);
  assert(!IS_ERR(d) && d->d_inode==&hidden);dput(d);
  if(cold)assert(last_lookup_flags==0);else assert(last_reval_flags==0);
  d=lookup_one_len("secret",&parent,6);assert(PTR_ERR(d)==-ENOENT);end();
 }
 const char *bad[]={"",".","..","a/b"};
 for(unsigned n=0;n<4;n++){begin();assert(PTR_ERR(susfs_lookup_one_len_metadata(bad[n],&parent,strlen(bad[n])))==-EACCES);assert(!lookup_calls);end();}
 begin();char embedded[]={'a',0,'b'};assert(PTR_ERR(susfs_lookup_one_len_metadata(embedded,&parent,3))==-EACCES);end();
 begin();permission_error=-EACCES;assert(PTR_ERR(susfs_lookup_one_len_metadata("secret",&parent,6))==-EACCES);assert(!lookup_calls);end();
 begin();parent.d_flags=DCACHE_OP_HASH;hash_error=-EINVAL;assert(PTR_ERR(susfs_lookup_one_len_metadata("secret",&parent,6))==-EINVAL);end();
 begin();reval=-ESTALE;assert(PTR_ERR(susfs_lookup_one_len_metadata("secret",&parent,6))==-ESTALE);assert(!cached_child.invalidated);end();
 for(unsigned n=0;n<4;n++) {
  begin();cache_miss=true;
  int expected=n==0?-ENOMEM:n==1?-EIO:n==2?-ENOENT:0;
  alloc_error=n==0;lookup_error=n==1?-EIO:0;directory.dead=n==2;if(n==3)resolved_inode=NULL;
  struct dentry *d=susfs_lookup_one_len_metadata("secret",&parent,6);
  assert(expected?PTR_ERR(d)==expected:!IS_ERR(d)&&!d->d_inode);release_result(d);end();
 }
 begin();sb.s_magic=0xef53;assert(PTR_ERR(susfs_lookup_one_len_metadata("secret",&parent,6))==-EXDEV);end();
#endif
}

static void test_shared_cache(void)
{
 for(unsigned cold=0;cold<3;cold++) {
  begin();cache_miss=cold!=0;lookup_alias=cold==2;
  struct ovl_cache_entry *p=new_entry("secret",22);p->ino=0;
  assert(ovl_cache_update_ino(&overlay_file.f_path,p)==0);assert(p->ino==10071 && gets==1);
  assert(cold?last_lookup_flags==0:last_reval_flags==0);
  uid.val=0;assert(ovl_cache_update_ino(&overlay_file.f_path,p)==0 && p->ino==10071);free(p);end();
 }
 begin();struct ovl_cache_entry *p=new_entry("secret",22);getattr_error=-EIO;
 assert(ovl_cache_update_ino(&overlay_file.f_path,p)==-EIO && p->ino==22);free(p);end();
 begin();p=new_entry("secret",22);same_sb=false;xino=8;origin=false;
 assert(!ovl_cache_update_ino(&overlay_file.f_path,p) && p->ino==(22ULL|(3ULL<<56)));free(p);end();
 begin();p=new_entry("secret",22);same_sb=false;assert(!ovl_cache_update_ino(&overlay_file.f_path,p) && p->ino==22 && !lookup_calls);free(p);end();
 begin();p=new_entry("secret",22);cache_miss=true;lookup_error=-EIO;assert(ovl_cache_update_ino(&overlay_file.f_path,p)==-EIO);free(p);end();
 begin();struct rb_root root;struct list_head entries;
 assert(!ovl_dir_read_impure(&overlay_file.f_path,&entries,&root));
 assert(entries.next!=&entries);p=list_entry(entries.next,struct ovl_cache_entry,l_node);assert(p->ino==10071 && p->real_ino==22);
 list_del(&p->l_node);free(p);end();
}

static void test_merged_iteration(void)
{
 for(unsigned cold=0;cold<3;cold++) {
  begin();cache_miss=cold!=0;lookup_alias=cold==2;
  add_entry("secret",0,false);add_entry("whiteout",7,true);add_entry(".",2,false);add_entry("..",3,false);
  struct recording r=recorder(8);assert(!ovl_iterate(&overlay_file,&r.ctx));
  assert(r.count==(enabled()?2:3) && r.ctx.pos==4);
  assert(cache.entries.next!=&cache.entries);struct ovl_cache_entry *p=list_entry(cache.entries.next,struct ovl_cache_entry,l_node);
  assert(p->ino==(enabled()?0:10071) && !p->is_whiteout);
  uid.val=0;r=recorder(8);ovl_seek_cursor(&od,0);assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==3 && r.inos[0]==10071);
  end();
 }
 for(unsigned exception=0;exception<4;exception++) {
  begin();add_entry("secret",42,false);
  if(exception==0)uid.val=0;if(exception==1)thread_flags=0;if(exception==2)hidden.i_uid=uid;if(exception==3)hidden_mapping.flags=0;
  struct recording r=recorder(8);assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1);end();
 }
 begin();add_entry("secret",42,false);add_entry("after",43,false);struct recording r=recorder(0);
 /* The first hidden entry advances; a visible entry stops without consuming its cookie. */
 cached_child.d_inode=&visible;resolved_inode=&visible;
 assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==0 && r.ctx.pos==0);
 r.limit=8;assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==2 && r.ctx.pos==2);end();
 begin();add_entry("secret",42,false);struct ovl_cache_entry *p=list_entry(cache.entries.next,struct ovl_cache_entry,l_node);
 r=recorder(8);assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==(enabled()?0:1) && r.ctx.pos==1);
 hidden_mapping.flags=0;r=recorder(8);ovl_seek_cursor(&od,0);assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1 && p->ino==42);end();
 begin();add_entry("secret",42,false);cache_miss=true;lookup_error=-ESTALE;r=recorder(8);
 assert(ovl_iterate(&overlay_file,&r.ctx)==(enabled()?-ESTALE:0) && r.count==(enabled()?0:1));end();
 begin();od.cache=NULL;cache_error=-ENOMEM;r=recorder(8);assert(ovl_iterate(&overlay_file,&r.ctx)==-ENOMEM);end();
 begin();od.is_real=model_real=true;xino=8;cache_miss=true;r=recorder(8);
 assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==(enabled()?1:2));
 assert(r.inos[r.count-1]==(23ULL|(3ULL<<56)));end();
}

static void user_directory(void)
{ begin();cache_miss=true;open_file=&overlay_file;add_entry("secret",71,false);add_entry("public",72,false); }
static void test_native_and_compat_records(void)
{
 _Alignas(16) unsigned char buffer[512];long result;
 user_directory();result=ksys_getdents64(3,(void *)buffer,sizeof(buffer));
 assert(result>0 && overlay_file.f_pos==2 && !ilookups);
 struct linux_dirent64 *d=(void *)buffer;assert(!strcmp(d->d_name,enabled()?"public":"secret"));
 if(enabled())assert(d->d_off==2 && result==d->d_reclen);
 result=ksys_getdents64(3,(void *)buffer,sizeof(buffer));assert(result==0);end();
 user_directory();result=sys_getdents(3,(void *)buffer,sizeof(buffer));assert(result>0 && overlay_file.f_pos==2);
 assert(!strcmp(((struct linux_dirent *)buffer)->d_name,enabled()?"public":"secret"));end();
 user_directory();result=sys_old_readdir(3,(void *)buffer,sizeof(buffer));assert(result==1);
 assert(!strcmp(((struct old_linux_dirent *)buffer)->d_name,enabled()?"public":"secret"));end();
#ifdef CONFIG_COMPAT
 user_directory();result=compat_sys_getdents(3,(void *)buffer,sizeof(buffer));assert(result>0 && overlay_file.f_pos==2);
 assert(!strcmp(((struct compat_linux_dirent *)buffer)->d_name,enabled()?"public":"secret"));end();
 user_directory();result=compat_sys_old_readdir(3,(void *)buffer,sizeof(buffer));assert(result==1);
 assert(!strcmp(((struct compat_old_linux_dirent *)buffer)->d_name,enabled()?"public":"secret"));end();
 begin();cache_miss=true;open_file=&overlay_file;add_entry("secret",1ULL<<40,false);
 result=compat_sys_getdents(3,(void *)buffer,sizeof(buffer));assert(result==(enabled()?0:-EOVERFLOW));end();
#endif
 begin();cache_miss=true;open_file=&overlay_file;add_entry("secret",0,false);memset(buffer,0xa5,sizeof(buffer));
 result=ksys_getdents64(3,(void *)buffer,1);assert(result==(enabled()?0:-EINVAL));
 if(enabled()){assert(!copies && !gets);for(unsigned n=0;n<sizeof(buffer);n++)assert(buffer[n]==0xa5);}
 end();
 user_directory();fault_after=0;assert(ksys_getdents64(3,(void *)buffer,sizeof(buffer))==-EFAULT);end();
 user_directory();security_error=-EACCES;assert(ksys_getdents64(3,(void *)buffer,sizeof(buffer))==-EACCES && !copies && !lookup_calls);end();
}

static void begin_direct(void)
{
 begin();od.is_real=model_real=true;od.cache=NULL;cache_miss=true;open_file=&overlay_file;
}

static void test_direct_pending_and_seek(void)
{
 begin_direct();struct recording r=recorder(8);
 assert(!ovl_iterate(&overlay_file,&r.ctx));
 assert(r.count==(enabled()?1:2) && r.ctx.pos==20 && !backing_actor_depth);
 overlay_file.f_pos=r.ctx.pos;
 assert(ovl_dir_llseek(&overlay_file,0,SEEK_SET)==0);
 uid.val=0;r=recorder(8);assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==2);end();
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
 begin_direct();r=recorder(0);assert(!ovl_iterate(&overlay_file,&r.ctx));
 assert(r.ctx.pos==10 && od.susfs_pending && real_file.f_pos==20);
 overlay_file.f_pos=r.ctx.pos;
 assert(ovl_dir_llseek(&overlay_file,0,SEEK_CUR)==10 && od.susfs_pending && real_file.f_pos==20);
 seek_error=-ESPIPE;
 assert(ovl_dir_llseek(&overlay_file,0,SEEK_SET)==-ESPIPE && overlay_file.f_pos==10 && od.susfs_pending);
 seek_error=0;
 assert(ovl_dir_llseek(&overlay_file,LLONG_MAX,SEEK_CUR)==-EINVAL && overlay_file.f_pos==10 && od.susfs_pending);
 r.limit=8;assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1 && !strcmp(r.names[0],"public") && r.ctx.pos==20 && !od.susfs_pending);end();
 begin_direct();r=recorder(0);assert(!ovl_iterate(&overlay_file,&r.ctx));overlay_file.f_pos=r.ctx.pos;
 assert(ovl_dir_llseek(&overlay_file,1,SEEK_CUR)==11 && real_file.f_pos==11 && !od.susfs_pending);
 r=recorder(8);r.ctx.pos=11;assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1 && !strcmp(r.names[0],"public"));end();
 /* A policy/owner change while a record waits must be checked at publication. */
 begin_direct();hidden_mapping.flags=0;r=recorder(0);assert(!ovl_iterate(&overlay_file,&r.ctx) && od.susfs_pending);
 hidden_mapping.flags=BIT(AS_FLAGS_SUS_PATH);r.limit=8;
 assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1 && !strcmp(r.names[0],"public"));end();
 begin_direct();hidden.i_uid=uid;r=recorder(0);assert(!ovl_iterate(&overlay_file,&r.ctx) && od.susfs_pending);
 uid.val=0;r.limit=8;assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==2 && !strcmp(r.names[0],"secret"));end();
 begin_direct();r=recorder(8);lookup_error=-ESTALE;
 assert(ovl_iterate(&overlay_file,&r.ctx)==-ESTALE && r.count==0 && od.susfs_pending && r.ctx.pos==0);
 lookup_error=0;assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1);end();
 begin_direct();r=recorder(8);permission_error=-EACCES;
 assert(ovl_iterate(&overlay_file,&r.ctx)==-EACCES && !r.count && od.susfs_pending);
 uid.val=0;assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==2);end();
 begin_direct();r=recorder(8);resolved_inode=NULL;
 assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1);end();
 begin_direct();r=recorder(8);backing_error=-EIO;
 assert(ovl_iterate(&overlay_file,&r.ctx)==-EIO && r.ctx.pos==10 && !r.count && !od.susfs_pending);
 backing_error=0;assert(!ovl_iterate(&overlay_file,&r.ctx) && r.count==1);end();
 begin_direct();hidden_mapping.flags=0;r=recorder(0);assert(!ovl_iterate(&overlay_file,&r.ctx) && od.susfs_pending);
 model_real=false;ovl_dir_reset(&overlay_file);assert(!od.is_real && !od.susfs_pending);end();
 begin_direct();struct ovl_susfs_collect collect={.ctx.actor=ovl_susfs_collect_one,.od=&od};
 char invalid[NAME_MAX+2];memset(invalid,'x',sizeof(invalid));
 assert(ovl_susfs_collect_one(&collect.ctx,invalid,NAME_MAX+1,0,1,8)==-EINVAL && collect.error==-EIO && !od.susfs_pending);
 char embedded[]={'a',0,'b'};collect.error=0;
 assert(ovl_susfs_collect_one(&collect.ctx,embedded,3,0,1,8)==-EINVAL && collect.error==-EIO);end();
#endif
}

static void test_direct_userspace_records(void)
{
 _Alignas(16) unsigned char buffer[512];long result;
 begin_direct();result=ksys_getdents64(3,(void *)buffer,sizeof(buffer));
 assert(result>0 && overlay_file.f_pos==20);
 assert(!strcmp(((struct linux_dirent64 *)buffer)->d_name,enabled()?"public":"secret"));
 assert(!ksys_getdents64(3,(void *)buffer,sizeof(buffer)));end();
 begin_direct();result=sys_getdents(3,(void *)buffer,sizeof(buffer));
 assert(result>0 && !strcmp(((struct linux_dirent *)buffer)->d_name,enabled()?"public":"secret"));end();
#ifdef CONFIG_COMPAT
 begin_direct();result=compat_sys_getdents(3,(void *)buffer,sizeof(buffer));
 assert(result>0 && !strcmp(((struct compat_linux_dirent *)buffer)->d_name,enabled()?"public":"secret"));end();
 begin_direct();xino=8;result=compat_sys_getdents(3,(void *)buffer,sizeof(buffer));
 assert(result==-EOVERFLOW);
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
 assert(od.susfs_pending && overlay_file.f_pos==10);
#endif
 end();
#endif
 begin_direct();fault_after=0;assert(ksys_getdents64(3,(void *)buffer,sizeof(buffer))==-EFAULT);
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
 assert(od.susfs_pending && overlay_file.f_pos==10);
#endif
 fault_after=100000;result=ksys_getdents64(3,(void *)buffer,sizeof(buffer));
 assert(result>0 && !strcmp(((struct linux_dirent64 *)buffer)->d_name,enabled()?"public":"secret"));end();
 begin_direct();impure=true;add_entry("secret",10071,false);add_entry("public",10072,false);
 struct recording r=recorder(8);assert(!ovl_iterate(&overlay_file,&r.ctx));
 assert(r.count==(enabled()?1:2) && r.inos[r.count-1]==10072);end();
}

int main(void)
{
 test_metadata_lookup();test_shared_cache();test_merged_iteration();test_native_and_compat_records();
 test_direct_pending_and_seek();test_direct_userspace_records();
 puts("Overlay merged/direct/translated iteration: deferred lookup, pending records, seeks, policy changes, native/compat copies/errors, shared-cache identity and references passed (host models)");
 return 0;
}
