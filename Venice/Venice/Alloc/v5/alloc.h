#ifndef __VNZ_ALLOC
#define __VNZ_ALLOC

#ifdef __cplusplus
extern "C"
{
#endif

#include <pthread.h>
#include <stdint.h>

    struct __vnza_block
    {
        uintptr_t __range_begin;
        uintptr_t __freeptr_local;
        /* __atomic */ uintptr_t __freeptr_global;

        uint16_t __object_size;
        uint16_t __object_count;
        /* __atomic */ uint16_t __allocation_count;
        uint8_t __padding1;
        uint8_t __padding2;

        uint64_t __thread_id;
        void (*__shadow_free) (struct __vnza_block *);
        void * __block_linkage;
        pthread_mutex_t __freeptr_lock;
    };

    struct __vnza_item
    {
        void * inner;
        struct __vnza_item * left;
        struct __vnza_item * right;
    };

    struct __vnza_linkage
    {
        struct __vnza_item * head;
        pthread_mutex_t lock;
        size_t __empties;
    };

    struct __vnza_heap
    {
        struct __vnza_linkage sized_linkages[20];
        struct __vnza_linkage empty_linkage;
    };

    struct __vnza_lut
    {
        struct __vnza_item ** linkages;
        size_t num_linkages;
    }

    int __vnza_init_local_heap (struct __vnza_heap *);
    int __vnza_destroy_local_heap (struct __vnza_heap *);

    extern uint64_t __vnza_get_local_thread_id ();
    extern struct __vnza_heap __vnza_local_heap__;

#ifdef __cplusplus
}
#endif

#endif /* !@__VNZ_ALLOC */
