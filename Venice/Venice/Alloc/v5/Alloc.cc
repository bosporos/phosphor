#include <Venice/Alloc/alloc.h>

uint64_t __vnza_get_local_thread_id__ ();

static thread_local uint64_t __vnza_local_thread_id__ = __vnza_get_local_thread_id__ ();

uint64_t __vnza_get_local_thread_id__ ()
{
    uint64_t temporary;
    pthread_threadid_np (NULL, &temporary);
    return temporary;
}

uint64_t __vnza_get_local_thread_id ()
{
    return __vnza_local_thread_id__;
}

static thread_local struct __vnza_local_heap_agent
{
    __vnza_local_heap_agent ()
    {
        __vnza_init_local_heap (&__vnza_local_heap__);
    }

    ~__vnza_local_heap_agent ()
    {
        __vnza_destroy_local_heap (&__vnza_local_heap__);
    }
} __vnza_local_heap_agent__ __vnza_tl_heap_agent;
