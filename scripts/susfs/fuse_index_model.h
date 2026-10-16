/* Host synchronization/hash surroundings for the real FUSE index source. */
#ifndef TEST_FUSE_INDEX_MODEL_H
#define TEST_FUSE_INDEX_MODEL_H
#include <pthread.h>
struct hlist_node { struct hlist_node *next, **pprev; };
struct hlist_head { struct hlist_node *first; };
#define HLIST_HEAD_INIT { .first = NULL }
#define WRITE_ONCE(p,v) __atomic_store_n(&(p),(v),__ATOMIC_RELAXED)
#define READ_ONCE(p) __atomic_load_n(&(p),__ATOMIC_RELAXED)
#define ARRAY_SIZE(v) (sizeof(v)/sizeof(*(v)))
#define ilog2(v) (63-__builtin_clzll(v))
#define GLOBAL_ROOT_UID ((kuid_t){0})
#define EXPORT_SYMBOL_GPL(v)
static unsigned hash_32(uint32_t key, unsigned bits) { return (key*0x61c88647U)>>(32-bits); }
static unsigned long hash_long(unsigned long key, unsigned bits) { return (key*0x61c8864680b583ebUL)>>(64-bits); }
#include "native_fuse_hash.h"
typedef pthread_mutex_t spinlock_t;
#define DEFINE_SPINLOCK(n) spinlock_t n = PTHREAD_MUTEX_INITIALIZER
static _Thread_local unsigned model_spin_depth;
static void spin_lock(spinlock_t *s) { assert(!pthread_mutex_lock(s)); model_spin_depth++; }
static void spin_unlock(spinlock_t *s) { assert(model_spin_depth); model_spin_depth--; assert(!pthread_mutex_unlock(s)); }
#define spin_lock_irqsave(s,f) do { (f)=0; spin_lock(s); } while(0)
#define spin_unlock_irqrestore(s,f) do { (void)(f); spin_unlock(s); } while(0)
#endif
