/* SPDX-License-Identifier: GPL-2.0 */
/* Actual vendor data-structure algorithms under a host fault-injecting allocator. */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int gfp_t;
#define PAGE_SIZE 4096
#define CONFIG_64BIT 1
#define BITS_PER_LONG 64
#define GFP_KERNEL 1
#define GFP_ATOMIC 2
#define __GFP_ZERO 4
#define FLEX_ARRAY_FREE 0x6c
#define SLAB_PANIC 0
#define __init
#define EXPORT_SYMBOL(x)
#define cond_resched() ((void)0)
#define BUG_ON(x) assert(!(x))
#define BUILD_BUG_ON(x) _Static_assert(!(x), #x)
#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))
/* Vendor Makefile saturates the version-code sublevel at 255. */
#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)
#define KSU_SUPPORT_ADD_TYPE 1
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define do_div(n,d) ((n)/=(d))
#define pr_warn(...) ((void)0)
#define pr_err(...) ((void)0)
struct netlbl_lsm_catmap;
static unsigned fls(u32 x) { return x ? 32-__builtin_clz(x) : 0; }
static unsigned find_next_bit(const unsigned long *p, unsigned size, unsigned offset)
{ for (unsigned i=offset; i<size; i++) if (p[i/64] & (1UL<<(i%64))) return i; return size; }
static unsigned find_first_bit(const unsigned long *p, unsigned size) { return find_next_bit(p,size,0); }
struct allocation {
    struct allocation *next, *prev;
    size_t bytes;
    unsigned magic;
    max_align_t alignment;
};
static struct allocation root = {.next = &root, .prev = &root};
static unsigned alloc_calls, fail_at;
static size_t live_blocks, live_bytes;
static void *host_alloc(size_t n, gfp_t flags)
{
    alloc_calls++;
    if (alloc_calls == fail_at) return NULL;
    assert(n <= SIZE_MAX-sizeof(struct allocation));
    struct allocation *h = malloc(sizeof(*h)+n); assert(h);
    h->bytes=n; h->magic=0x12345678; h->next=root.next; h->prev=&root;
    root.next->prev=h; root.next=h;
    memset(h+1,(flags&__GFP_ZERO)?0:0xa5,n);
    live_blocks++; live_bytes+=n; return h+1;
}
static void kfree(void *p)
{
    if (!p) return;
    struct allocation *h=(struct allocation *)p-1;
    assert(h->magic==0x12345678 && live_blocks);
    h->prev->next=h->next; h->next->prev=h->prev;
    live_blocks--; live_bytes-=h->bytes; h->magic=0; free(h);
}
static void *kmalloc(size_t n, gfp_t f) { return host_alloc(n,f); }
static void *kzalloc(size_t n, gfp_t f) { return host_alloc(n,f|__GFP_ZERO); }
static void *kmalloc_array(size_t n, size_t s, gfp_t f)
{ return s && n>SIZE_MAX/s ? NULL : host_alloc(n*s,f); }
static void *kcalloc(size_t n, size_t s, gfp_t f) { return kmalloc_array(n,s,f|__GFP_ZERO); }
static char *kstrdup(const char *s, gfp_t f)
{ size_t n=strlen(s)+1; char *p=host_alloc(n,f); if(p) memcpy(p,s,n); return p; }
struct kmem_cache { size_t size; };
static struct kmem_cache hash_cache, bitmap_cache;
static struct kmem_cache *kmem_cache_create(const char *s, size_t n, unsigned a, unsigned f, void *c)
{ hash_cache.size=n; return &hash_cache; }
static void *kmem_cache_zalloc(struct kmem_cache *c, gfp_t f) { assert(c); return kzalloc(c->size,f); }
static void kmem_cache_free(struct kmem_cache *c, void *p) { kfree(p); }
#include "vendor_reciprocal_types.h"
#include "vendor_reciprocal.h"
#define flex_array_put real_flex_array_put
#include "vendor_flex_types.h"
#include "vendor_flex_array.h"
#undef flex_array_put
static unsigned put_calls, fail_put_at;
static int flex_array_put(struct flex_array *fa, unsigned i, void *src, gfp_t flags)
{ put_calls++; return put_calls == fail_put_at ? -ENOMEM : real_flex_array_put(fa,i,src,flags); }
#include "vendor_ebitmap_types.h"
static struct kmem_cache *ebitmap_node_cachep=&bitmap_cache;
#include "vendor_ebitmap.h"
#include "vendor_hashtab_types.h"
#include "vendor_hashtab.h"
#include "vendor_symtab_types.h"
#include "vendor_policy_types.h"
#define SYM_ROLES 1
#define SYM_TYPES 3
#define SYM_NUM 8
struct policydb {
    struct symtab symtab[SYM_NUM];
    struct flex_array *sym_val_to_name[SYM_NUM];
    struct role_datum **role_val_to_struct;
    struct flex_array *type_val_to_struct_array;
    struct flex_array *type_attr_map_array;
};
#define p_types symtab[SYM_TYPES]
#define p_roles symtab[SYM_ROLES]
#define symtab_search(s,n) hashtab_search((s)->table,n)
#define symtab_insert(s,k,v) hashtab_insert((s)->table,k,v)
#ifdef BASELINE_TYPE_CREATION
#include "bakasu_baseline_type_creation.h"
#else
#include "bakasu_type_creation.h"
#endif
_Static_assert(sizeof(struct flex_array)==PAGE_SIZE,"native flex base must be one 4K page");
_Static_assert(sizeof(struct ebitmap_node)==64,"native 64-bit bitmap node geometry");
static u32 key_hash(struct hashtab *h, const void *k)
{ const unsigned char *s=k; u32 v=0; while(*s) v=v*33+*s++; return v%h->size; }
static int key_cmp(struct hashtab *h, const void *a, const void *b) { return strcmp(a,b); }
static int destroy_symbol(void *key, void *datum, void *unused) { kfree(key); kfree(datum); return 0; }
static struct policydb make_policy(u32 count, u32 spare, u32 roles)
{
    assert(!live_blocks); fail_at=fail_put_at=0;
    struct policydb p={0}; p.p_types.nprim=count; p.p_roles.nprim=roles;
    p.p_types.table=hashtab_create(key_hash,key_cmp,64); assert(p.p_types.table);
    p.type_attr_map_array=flex_array_alloc(sizeof(struct ebitmap),count+spare,GFP_KERNEL|__GFP_ZERO);
    p.type_val_to_struct_array=flex_array_alloc(sizeof(struct type_datum *),count+spare,GFP_KERNEL|__GFP_ZERO);
    p.sym_val_to_name[SYM_TYPES]=flex_array_alloc(sizeof(char *),count+spare,GFP_KERNEL|__GFP_ZERO);
    assert(p.type_attr_map_array && p.type_val_to_struct_array && p.sym_val_to_name[SYM_TYPES]);
    assert(!flex_array_prealloc(p.type_attr_map_array,0,count+spare,GFP_KERNEL|__GFP_ZERO));
    assert(!flex_array_prealloc(p.type_val_to_struct_array,0,count+spare,GFP_KERNEL|__GFP_ZERO));
    assert(!flex_array_prealloc(p.sym_val_to_name[SYM_TYPES],0,count+spare,GFP_KERNEL|__GFP_ZERO));
    for (u32 i=0;i<count;i++) {
        char label[32]; snprintf(label,sizeof(label),"old_%u",i);
        struct type_datum *t=kzalloc(sizeof(*t),GFP_KERNEL); char *name=kstrdup(label,GFP_KERNEL);
        assert(t && name); t->value=i+1; t->primary=1;
        assert(!hashtab_insert(p.p_types.table,name,t));
        assert(!flex_array_put_ptr(p.type_val_to_struct_array,i,t,GFP_KERNEL));
        assert(!flex_array_put_ptr(p.sym_val_to_name[SYM_TYPES],i,name,GFP_KERNEL));
        struct ebitmap e; ebitmap_init(&e); assert(!ebitmap_set_bit(&e,i,1));
        assert(!flex_array_put(p.type_attr_map_array,i,&e,GFP_KERNEL));
    }
    if (roles) {
        p.role_val_to_struct=kcalloc(roles,sizeof(*p.role_val_to_struct),GFP_KERNEL);
        assert(p.role_val_to_struct);
        for (u32 i=0;i<roles;i++) {
            p.role_val_to_struct[i]=kzalloc(sizeof(struct role_datum),GFP_KERNEL);
            assert(p.role_val_to_struct[i]); p.role_val_to_struct[i]->value=i+1;
            assert(!ebitmap_set_bit(&p.role_val_to_struct[i]->types,0,1));
            if(count) assert(!ebitmap_set_bit(&p.role_val_to_struct[i]->types,count-1,1));
        }
    }
    alloc_calls=put_calls=0;
    return p;
}
static void destroy_policy(struct policydb *p)
{
    for(u32 i=0;i<p->p_types.nprim;i++) {
        struct ebitmap *e=flex_array_get(p->type_attr_map_array,i); if(e) ebitmap_destroy(e);
    }
    flex_array_free(p->type_attr_map_array); flex_array_free(p->type_val_to_struct_array);
    flex_array_free(p->sym_val_to_name[SYM_TYPES]);
    hashtab_map(p->p_types.table,destroy_symbol,NULL); hashtab_destroy(p->p_types.table);
    for(u32 i=0;i<p->p_roles.nprim;i++) { ebitmap_destroy(&p->role_val_to_struct[i]->types); kfree(p->role_val_to_struct[i]); }
    kfree(p->role_val_to_struct);
    assert(!live_blocks && !live_bytes && root.next==&root);
}
struct snapshot {
    u32 count, hash_count;
    struct flex_array *maps,*types,*names;
    struct ebitmap_node *role_nodes[4];
    u32 role_highbits[4];
    size_t blocks,bytes;
};
static struct snapshot snapshot(struct policydb *p)
{
    struct snapshot s={.count=p->p_types.nprim,.hash_count=p->p_types.table->nel,
        .maps=p->type_attr_map_array,.types=p->type_val_to_struct_array,.names=p->sym_val_to_name[SYM_TYPES],
        .blocks=live_blocks,.bytes=live_bytes};
    assert(p->p_roles.nprim<=4);
    for(u32 i=0;i<p->p_roles.nprim;i++) { s.role_nodes[i]=p->role_val_to_struct[i]->types.node; s.role_highbits[i]=p->role_val_to_struct[i]->types.highbit; }
    return s;
}
static void check_old_entries(struct policydb *p, u32 count)
{
    for(u32 i=0;i<count;i++) {
        char label[32]; snprintf(label,sizeof(label),"old_%u",i);
        struct type_datum *t=flex_array_get_ptr(p->type_val_to_struct_array,i);
        char *name=flex_array_get_ptr(p->sym_val_to_name[SYM_TYPES],i);
        assert(t && t->value==i+1 && t->primary && !t->attribute && !strcmp(name,label));
        assert(hashtab_search(p->p_types.table,label)==t);
        struct ebitmap *e=flex_array_get(p->type_attr_map_array,i); assert(e && ebitmap_get_bit(e,i));
    }
}
static void unchanged(struct policydb *p, const struct snapshot *s)
{
    assert(p->p_types.nprim==s->count && p->p_types.table->nel==s->hash_count);
    assert(p->type_attr_map_array==s->maps && p->type_val_to_struct_array==s->types && p->sym_val_to_name[SYM_TYPES]==s->names);
    assert(!symtab_search(&p->p_types,"new_type"));
    assert(live_blocks==s->blocks && live_bytes==s->bytes);
    for(u32 i=0;i<p->p_roles.nprim;i++) {
        struct ebitmap *e=&p->role_val_to_struct[i]->types;
        assert(e->node==s->role_nodes[i] && e->highbit==s->role_highbits[i]);
        assert(ebitmap_get_bit(e,0) && (!s->count || ebitmap_get_bit(e,s->count-1)));
        assert(!ebitmap_get_bit(e,s->count));
    }
    check_old_entries(p,s->count);
}
static void success(struct policydb *p,u32 old_count,bool attr)
{
    assert(p->p_types.nprim==old_count+1 && p->p_types.table->nel==old_count+1);
    struct type_datum *t=symtab_search(&p->p_types,"new_type");
    assert(t && t->value==old_count+1 && t->primary && t->attribute==attr);
    assert(flex_array_get_ptr(p->type_val_to_struct_array,old_count)==t);
    assert(!strcmp(flex_array_get_ptr(p->sym_val_to_name[SYM_TYPES],old_count),"new_type"));
    assert(ebitmap_get_bit(flex_array_get(p->type_attr_map_array,old_count),old_count));
    for(u32 i=0;i<p->p_roles.nprim;i++) assert(ebitmap_get_bit(&p->role_val_to_struct[i]->types,old_count));
    check_old_entries(p,old_count);
}
static unsigned total_failures;
static void allocation_matrix(u32 count,u32 spare,u32 roles,bool attr)
{
    struct policydb p=make_policy(count,spare,roles);
    assert(add_type(&p,"new_type",attr)); unsigned n=alloc_calls; success(&p,count,attr); destroy_policy(&p);
    for(unsigned i=1;i<=n;i++) {
        p=make_policy(count,spare,roles); struct snapshot s=snapshot(&p); fail_at=i;
        assert(!add_type(&p,"new_type",attr)); unchanged(&p,&s);
        /* Retry the same policy to prove failure did not poison its state. */
        fail_at=0; alloc_calls=0; assert(add_type(&p,"new_type",attr)); success(&p,count,attr);
        destroy_policy(&p); total_failures++;
    }
    printf("PASS: %u allocation failures and retries, types=%u spare=%u roles=%u attribute=%u\n",n,count,spare,roles,attr);
}
static void put_matrix(void)
{
    struct policydb p=make_policy(2,0,2); assert(add_type(&p,"new_type",false)); unsigned n=put_calls; destroy_policy(&p);
    for(unsigned i=1;i<=n;i++) {
        p=make_policy(2,0,2); struct snapshot s=snapshot(&p); fail_put_at=i;
        assert(!add_type(&p,"new_type",false)); unchanged(&p,&s); destroy_policy(&p);
    }
    printf("PASS: %u injected flex-array write errors roll back without freeing borrowed bitmap nodes\n",n);
}
static void boundaries(void)
{
    struct policydb p=make_policy(2,0,2); struct snapshot s=snapshot(&p);
    fail_at=1; assert(add_type(&p,"old_0",false) && !alloc_calls); unchanged(&p,&s); fail_at=0;
    assert(!add_type(NULL,"new_type",false)); assert(!add_type(&p,NULL,false)); assert(!add_type(&p,"",false)); unchanged(&p,&s);
    p.p_types.nprim=INT_MAX; assert(!add_type(&p,"new_type",false)); p.p_types.nprim=s.count; unchanged(&p,&s);
    p.type_val_to_struct_array->total_nr_elements=1; assert(!add_type(&p,"new_type",false)); p.type_val_to_struct_array->total_nr_elements=2; unchanged(&p,&s);
    p.type_attr_map_array->element_size=1; assert(!add_type(&p,"new_type",false)); p.type_attr_map_array->element_size=sizeof(struct ebitmap); unchanged(&p,&s);
    destroy_policy(&p);
    puts("PASS: existing symbol is allocation-free; invalid names/counts/layout rejected without mutation");
}
int main(void)
{
    hashtab_cache_init(); bitmap_cache.size=sizeof(struct ebitmap_node);
#ifdef BASELINE_TYPE_CREATION
    for(unsigned i=1;i<=3;i++) {
        struct policydb p=make_policy(1,0,2); struct snapshot s=snapshot(&p); fail_at=i;
        assert(!add_type(&p,"new_type",false));
        assert(p.p_types.nprim==s.count+1 && p.p_types.table->nel==s.hash_count);
        assert(live_blocks==s.blocks+i-1);
        /* Restore only the host fixture for cleanup of this known-broken baseline. */
        p.p_types.nprim=s.count;
        while(live_blocks>s.blocks) kfree(root.next+1);
        destroy_policy(&p);
    }
    puts("EXPECTED NEGATIVE: pinned baseline changes the type count on three allocation failures and leaks type/key allocations");
    return 0;
#else
    allocation_matrix(1,0,2,false);
    allocation_matrix(FLEX_ARRAY_BASE_BYTES_LEFT/sizeof(struct ebitmap),0,2,false);
    allocation_matrix(FLEX_ARRAY_BASE_BYTES_LEFT/sizeof(void *),0,2,false);
    allocation_matrix(EBITMAP_SIZE,0,2,true);
    allocation_matrix(EBITMAP_SIZE*2,0,2,false);
    allocation_matrix(2,1000,2,false);
    allocation_matrix(1,0,0,true);
    put_matrix(); boundaries();
    printf("PASS: %u total allocation-failure paths using actual Motorola primitives; no sanitizer errors or leaked blocks\n",total_failures);
    return 0;
#endif
}
