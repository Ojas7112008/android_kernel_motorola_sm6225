/* SPDX-License-Identifier: GPL-2.0 */
/* Actual input/provider bodies with host locks and recorded native delivery. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#define __user
#define likely(v) (v)
#define pr_info(...) ((void)0)
struct task_struct { int unused; };
struct kuid { uid_t val; };
static struct kuid current_uid(void) { return (struct kuid){0}; }
static int test_thread_flag(int bit) { return 0; }
static void set_thread_flag(int bit) {}
static void clear_thread_flag(int bit) {}
static void clear_tsk_thread_flag(struct task_struct *t, int bit) {}
#ifdef CONFIG_KSU_SUSFS
#include <linux/ksu_susfs.h>
#endif
#include "input_event_codes.h"
#include "input_constants.h"
#define DEFINE_SPINLOCK(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
static _Thread_local int irq_depth;
#define spin_lock_irqsave(lock, flags) do { (flags) = irq_depth++; assert(!pthread_mutex_lock(lock)); } while (0)
#define spin_unlock_irqrestore(lock, flags) do { assert(irq_depth == (int)(flags) + 1); irq_depth = (flags); assert(!pthread_mutex_unlock(lock)); } while (0)
struct static_key { atomic_bool enabled; };
static struct static_key ksu_is_input_hook_enabled = {true};
static atomic_int disable_calls;
static bool static_key_enabled(struct static_key *key) { return atomic_load(&key->enabled); }
static bool static_branch_likely(struct static_key *key) { return static_key_enabled(key); }
static void static_branch_disable(struct static_key *key) {
    assert(!irq_depth); atomic_fetch_add(&disable_calls, 1); atomic_store(&key->enabled, false);
}
#define ksu_input_hook_inactive() (!static_branch_likely(&ksu_is_input_hook_enabled))
static bool ksu_late_loaded;
static void ksu_stop_input_hook_runtime(void);
#include "provider_stop_input.h"
#if defined(CONFIG_KSU_SUSFS) && defined(PROVIDER_BASELINE_NEGATIVE)
#include "provider_input_baseline.h"
#elif defined(CONFIG_KSU_SUSFS)
#include "provider_input.h"
static bool mutate_hook;
int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code, int *value) {
    int result = actual_input_hook(type, code, value);
    if (mutate_hook) { *type = EV_SYN; *code = SYN_REPORT; *value = 99; return -ENOSYS; }
    return result;
}
#endif
#define BITS (KEY_MAX / (8 * sizeof(unsigned long)) + 1)
struct input_value { unsigned int type, code; int value; };
struct input_mt { int slot; };
struct input_dev {
    pthread_mutex_t event_lock;
    unsigned long evbit[BITS], keybit[BITS], key[BITS], swbit[BITS], sw[BITS];
    unsigned long absbit[BITS], relbit[BITS], mscbit[BITS], ledbit[BITS], led[BITS], sndbit[BITS], snd[BITS];
    int rep[REP_MAX + 1];
    void (*event)(struct input_dev *, unsigned int, unsigned int, int);
    struct input_value storage[64], delivered[2048], *vals;
    unsigned int num_vals, max_vals, delivered_count, entropy_count, device_count;
    struct input_mt *mt;
    long timestamp[1];
};
#define INPUT_CLK_MONO 0
static const struct input_value input_value_sync = { EV_SYN, SYN_REPORT, 1 };
static _Thread_local struct input_dev *active_dev;
static int test_bit(unsigned int code, unsigned long *map) { return !!(map[code/(8*sizeof(*map))] & (1UL << (code%(8*sizeof(*map))))); }
static void __change_bit(unsigned int code, unsigned long *map) { map[code/(8*sizeof(*map))] ^= 1UL << (code%(8*sizeof(*map))); }
static void set_bit(unsigned int code, unsigned long *map) { map[code/(8*sizeof(*map))] |= 1UL << (code%(8*sizeof(*map))); }
/* ABS filtering is a mock boundary; the extracted caller receives its normalized value. */
static int input_handle_abs_event(struct input_dev *dev, unsigned int code, int *value) { *value = 3; return INPUT_PASS_TO_HANDLERS; }
static void add_input_randomness(unsigned int type, unsigned int code, int value) {
    assert(irq_depth == 1 && active_dev && type != EV_SYN); active_dev->entropy_count++;
}
static void device_event(struct input_dev *dev, unsigned int type, unsigned int code, int value) { assert(irq_depth == 1); dev->device_count++; }
static void input_pass_values(struct input_dev *dev, struct input_value *vals, unsigned int count) {
    assert(irq_depth == 1 && dev->delivered_count + count < 2048);
    memcpy(dev->delivered + dev->delivered_count, vals, count * sizeof(*vals)); dev->delivered_count += count;
}
static long ktime_set(int s, int ns) { return 0; }
#include "native_input.h"
static void init_dev(struct input_dev *dev) {
    memset(dev, 0, sizeof(*dev)); assert(!pthread_mutex_init(&dev->event_lock, NULL));
    dev->vals = dev->storage; dev->max_vals = 64; dev->event = device_event;
    unsigned int types[] = {EV_KEY, EV_REL, EV_ABS, EV_MSC, EV_SYN};
    for (size_t i = 0; i < sizeof(types)/sizeof(*types); i++) set_bit(types[i], dev->evbit);
    set_bit(KEY_VOLUMEDOWN, dev->keybit); set_bit(KEY_VOLUMEUP, dev->keybit);
    set_bit(REL_X, dev->relbit); set_bit(ABS_X, dev->absbit); set_bit(MSC_SCAN, dev->mscbit);
}
static void event(struct input_dev *dev, unsigned int type, unsigned int code, int value) {
    active_dev = dev; input_event(dev, type, code, value); active_dev = NULL; assert(!irq_depth);
}
#if defined(CONFIG_KSU_SUSFS) && !defined(PROVIDER_BASELINE_NEGATIVE)
static void reset_provider(void) {
    assert(!irq_depth); volumedown_pressed_count = 0; ksu_devon_input_stopped = false;
    atomic_store(&ksu_is_input_hook_enabled.enabled, true); atomic_store(&disable_calls, 0);
    mutate_hook = ksu_late_loaded = false;
}
static unsigned int count(void) {
    unsigned long flags; unsigned int n;
    spin_lock_irqsave(&ksu_devon_input_lock, flags); n = volumedown_pressed_count;
    spin_unlock_irqrestore(&ksu_devon_input_lock, flags); return n;
}
#endif
static void native_tests(void) {
    struct input_dev dev; init_dev(&dev);
#if defined(CONFIG_KSU_SUSFS) && !defined(PROVIDER_BASELINE_NEGATIVE)
    reset_provider(); mutate_hook = true;
#endif
    event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1);
    event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1); /* duplicate filtered by native state */
    event(&dev, EV_KEY, KEY_VOLUMEDOWN, 2); /* accepted repeat, not a fresh press */
    event(&dev, EV_KEY, KEY_VOLUMEDOWN, 0);
    event(&dev, EV_KEY, KEY_MAX + 1, 1); /* unsupported */
    event(&dev, EV_KEY, KEY_VOLUMEUP, 1);
    event(&dev, EV_REL, REL_X, 0); /* filtered */
    event(&dev, EV_REL, REL_X, 5);
    event(&dev, EV_ABS, ABS_X, 4);
    event(&dev, EV_MSC, MSC_SCAN, 22);
    event(&dev, EV_SYN, SYN_REPORT, 0);
    assert(dev.entropy_count == 7 && dev.device_count == 1 && dev.delivered_count == 8);
    struct input_value expected[] = {{EV_KEY,KEY_VOLUMEDOWN,1},{EV_KEY,KEY_VOLUMEDOWN,2},
        {EV_KEY,KEY_VOLUMEDOWN,0},{EV_KEY,KEY_VOLUMEUP,1},{EV_REL,REL_X,5},
        {EV_ABS,ABS_X,3},{EV_MSC,MSC_SCAN,22},{EV_SYN,SYN_REPORT,0}};
    for (unsigned int i = 0; i < 8; i++) {
        assert(dev.delivered[i].type == expected[i].type && dev.delivered[i].code == expected[i].code && dev.delivered[i].value == expected[i].value);
    }
#if defined(CONFIG_KSU_SUSFS) && !defined(PROVIDER_BASELINE_NEGATIVE)
    assert(count() == 1 && !atomic_load(&disable_calls));
    assert(!ksu_is_safe_mode()); assert(atomic_load(&disable_calls) == 1);
    event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1); assert(count() == 1);
    assert(!ksu_is_safe_mode() && atomic_load(&disable_calls) == 1);
#endif
    /* Null early buffer still updates key state and one entropy contribution. */
    struct input_dev early; init_dev(&early); early.vals = NULL;
    event(&early, EV_KEY, KEY_VOLUMEDOWN, 1); assert(early.entropy_count == 1 && !early.delivered_count);
    assert(!pthread_mutex_destroy(&early.event_lock)); assert(!pthread_mutex_destroy(&dev.event_lock));
}
#if defined(CONFIG_KSU_SUSFS) && !defined(PROVIDER_BASELINE_NEGATIVE)
static void threshold_tests(void) {
    struct input_dev dev; init_dev(&dev); reset_provider();
    event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1);
    for (int i = 0; i < 40; i++) event(&dev, EV_KEY, KEY_VOLUMEDOWN, 2);
    assert(count() == 1 && !atomic_load(&disable_calls));
    for (int i = 0; i < 20; i++) { event(&dev, EV_KEY, KEY_VOLUMEDOWN, 0); event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1); }
    assert(count() == 3 && !atomic_load(&disable_calls));
    assert(ksu_is_safe_mode() && ksu_is_safe_mode()); assert(atomic_load(&disable_calls) == 1);
    event(&dev, EV_KEY, KEY_VOLUMEDOWN, 0); event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1); assert(count() == 3);
    reset_provider(); ksu_late_loaded = true;
    assert(!ksu_is_safe_mode() && !atomic_load(&disable_calls)); ksu_late_loaded = false;
    ksu_stop_input_hook_runtime(); event(&dev, EV_KEY, KEY_VOLUMEDOWN, 0); event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1);
    assert(count() == 0 && !ksu_is_safe_mode()); assert(atomic_load(&disable_calls) == 1);
    assert(!pthread_mutex_destroy(&dev.event_lock));
}
static pthread_barrier_t barrier;
static void *press_worker(void *ptr) {
    struct input_dev dev; init_dev(&dev);
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < 100; i++) { event(&dev, EV_KEY, KEY_VOLUMEDOWN, 1); event(&dev, EV_KEY, KEY_VOLUMEDOWN, 0); }
    assert(dev.entropy_count == 200); assert(!pthread_mutex_destroy(&dev.event_lock)); return NULL;
}
static void *stop_worker(void *ptr) {
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < 20; i++) ksu_stop_input_hook_runtime();
    return NULL;
}
static void concurrency_tests(void) {
    pthread_t threads[8]; reset_provider(); assert(!pthread_barrier_init(&barrier,NULL,8));
    for (int i = 0; i < 8; i++) assert(!pthread_create(&threads[i],NULL,press_worker,NULL));
    for (int i = 0; i < 8; i++) assert(!pthread_join(threads[i],NULL));
    assert(!pthread_barrier_destroy(&barrier)); assert(count() == 3 && ksu_is_safe_mode());
    assert(atomic_load(&disable_calls) == 1);
    reset_provider(); volumedown_pressed_count = 2; assert(!pthread_barrier_init(&barrier,NULL,8));
    for (int i = 0; i < 8; i++) assert(!pthread_create(&threads[i],NULL,i < 4 ? press_worker : stop_worker,NULL));
    for (int i = 0; i < 8; i++) assert(!pthread_join(threads[i],NULL));
    assert(!pthread_barrier_destroy(&barrier));
    unsigned int frozen = count(); assert(frozen == 2 || frozen == 3);
    for (int i = 0; i < 40; i++) assert(ksu_is_safe_mode() == (frozen == 3));
    assert(count() == frozen && atomic_load(&disable_calls) == 1);
}
#endif
int main(void) {
#ifdef PROVIDER_BASELINE_NEGATIVE
    unsigned int type = EV_KEY, code = KEY_VOLUMEDOWN; int value = 1;
    ksu_handle_input_handle_event(&type,&code,&value); value = 2;
    ksu_handle_input_handle_event(&type,&code,&value); ksu_handle_input_handle_event(&type,&code,&value);
    assert(is_volumedown_enough(volumedown_pressed_count));
    puts("EXPECTED NEGATIVE: pinned raw hook counts one press plus two repeats as the three-press safe-mode threshold");
#else
    native_tests();
#ifdef CONFIG_KSU_SUSFS
    threshold_tests(); concurrency_tests();
#endif
#ifdef CONFIG_KSU_SUSFS
    puts("PASS: extracted native input filtering/delivery/entropy preserved; target real-press threshold, process-only stop and concurrent cutoff pass under host locks");
#else
    puts("PASS: extracted native input filtering/delivery/entropy preserved with the SUSFS input hook compiled out");
#endif
#endif
    return 0;
}
