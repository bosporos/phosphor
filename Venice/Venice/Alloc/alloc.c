#include <Venice/Alloc/alloc.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <threads.h>

_Thread_local struct __vnza_heap __vnza_local_heap__;

extern int64_t __vnza_chain_lookup (uint64_t size);

struct __vnza_linkage * __vnza_get_linkage_for_size (struct __vnza_heap * heap, uint64_t size)
{
    int index = (int)__vnza_chain_lookup (size);
    return &heap->sized_linkages[index];
}

struct __vnza_heap __vnza_global_heap__;

struct __vnza_heap * __vnza_get_global_heap ()
{
    return &__vnza_global_heap__;
}

void * __vnza_backup_allocate (size_t size)
{
    return malloc (size);
}

void __vnza_backup_free (void * object)
{
    free (object);
}

static __vnza_lut __vnza_global_lut__;

struct __vnza_lut * __vnza_get_lut ()
{
    return &__vnza_global_lut__;
}

uint64_t __vnza_lut_index (void * object, struct __vnza_lut * lut)
{
    return (uint64_t) (((uintptr_t)object / 0x4000) % lut->num_linkages);
}

int __vnza_add_item_to_linkage (struct __vnza_item * item, struct __vnza_linkage * linkage)
{
    pthread_mutex_lock (&linkage->lock);
    if (linkage->head != NULL) {
        item->left           = linkage->head;
        linkage->head->right = item;
        item->right          = linkage->head->right;
        if (item->right != NULL)
            item->right->left = item;
    } else {
        linkage->head = item;
        item->left    = NULL;
        item->right   = NULL;
    }
    pthread_mutex_unlock (&linkage->lock);
}

void __vnza_add_to_lut (struct __vnza_block * block, struct __vnza_lut * lut)
{

    uint64_t idx                 = __vnza_lut_index (block->__range_begin, lut);
    struct __vnza_item * linkage = lut->linkages[idx];

    __vnza_add_item_to_linkage (__vnza__alloc_item (block), linkage);
}

void * __vnza_alloc_from_block (struct __vnza_block * block)
{
    if (__atomic_fetch_add (&block->__allocation_count, 1, __ATOMIC_ACQ_REL) <= block->__object_count) {
        __atomic_add_fetch (&block->__allocation_count, 1, __ATOMIC_ACQ_REL);
        if (block->__freeptr_local != (uintptr_t)NULL) {
            goto __local_block_alloc;
        } else {
            pthread_mutex_lock (&block->__freeptr_lock);
            uintptr_t _global
                = __atomic_exchange_n (&block->__freeptr_global, NULL, __ATOMIC_ACQ_REL);
            pthread_mutex_unlock (&block->__freeptr_lock);
            block->__freeptr_local = _global;
            if (block->__freeptr_local != (uintptr_t)NULL) {
                goto __local_block_alloc;
            } else {
                return NULL;
            }
        }
    } else {
        __atomic_sub_fetch (&block->__allocation_count, 1, __ATOMIC_ACQ_REL);
        return NULL;
    }
__local_block_alloc:;
    void * object   = (void *)block->__freeptr_local;
    uint16_t offset = *(uint16_t *)object;
    if (!offset) {
        block->__freeptr_local = block->__range_begin + offset;
    } else {
        block->__freeptr_local = (uintptr_t)NULL;
    }
    return object;
}

void __vnza_format_block (struct __vnza_block * block, uintptr_t size)
{
    block->__object_size  = size;
    block->__object_count = 0x4000 / size;
    uintptr_t byte_ptr    = block->__range_begin;
    uint16_t index        = 1;
    while (index < block->__object_count) {
        *(uint16_t *)byte_ptr = index * size;
        byte_ptr += size;
        ++index;
    }
    *(uint16_t *)byte_ptr = 0xffff;
}

int __vnza_alloc_block (struct __vnza_block ** _block, uintptr_t size)
{
    *_block                     = malloc (sizeof (struct __vnza_block));
    struct __vnza_block * block = *_block;
    void * mapping              = mmap (NULL, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_SHARED, 0, 0);
    if (__builtin_expect (MAP_FAILED == mapping, 0)) {
        return -1;
    } else {
        block->__range_begin    = (uintptr_t)mapping;
        block->__freeptr_local  = block->__range_begin;
        block->__freeptr_global = (uintptr_t)NULL;
        __vnza_format_block (block, size);
        block->__thread_id = __vnza_get_local_thread_id ();

        block->__freptr_lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init (&attr);
        pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init (&this->__freeptr_lock, &attr);
        pthread_mutexattr_destroy (&attr);

        __vnza_add_to_lut (block, __vnza_get_lut ());
    }

    return 0;
}

void __vnza_free_block (struct __vnza_block * block)
{
    /* debug_assert_eq( */ munmap (block) /*, 0) */;
}

int __vnza_create_item (struct __vnza_item ** _item, struct __vnza_block * block)
{
    *_item = malloc (24);
    if (*_item == NULL)
        return -1;
    (*_item)->left  = NULL;
    (*_item)->right = NULL;
    (*_item)->inner = block;

    return 0;
}

void * __vnza_alloc (uint64_t size)
{
    if (_local_heap == NULL /* 0 (tss_get error condition) == NULL */) {
        return NULL;
    }
    if (size <= 0x2000) {
        const int linkage_index         = __vnza_chain_lookup (size);
        struct __vnza_linkage * linkage = &__vnza_local_heap__->sized_linkages[linkage_index];

        void * object = __vnza_alloc_from_block (
            __atomic_load_n (&linkage->head, __ATOMIC_ACQUIRE)->inner);
        if (object != NULL) {
            return object;
        }
        pthread_mutex_lock (&linkage->lock);
        // if (linkage->head == NULL) {
        //     // TODO: actually go through the unsized linkage & the global heap's
        //     goto __pull_from_global;
        // }
        if (linkage->head != NULL)
            if (linkage->head->right != NULL) {
                __atomic_store (&linkage->head, &linkage->head->right, __ATOMIC_ACQ_REL);
                object = __vnza_alloc_from_block (
                    __atomic_load_n (&linkage->head, __ATOMIC_ACQUIRE)->inner);
                pthread_mutex_unlock (&linkage->lock);
                return object;
            }
        // Aight, nothing to the right
        struct __vnza_linkage * empty_linkage = &__vnza_local_heap__->empty_linkage;
        pthread_mutex_lock (&empty_linkage->lock);
        if (empty_linkage->head != NULL) {
            struct __vnza_item * link = empty_linkage->head;
            empty_linkage->head       = empty_linkage->head->right;
            if (empty_linkage->head != NULL)
                empty_linkage->head->left = NULL;
            pthread_mutex_unlock (&empty_linkage->lock);
            // removed from the empty linkage
            link->left = __atomic_load_n (&linkage->head, __ATOMIC_ACQUIRE);
            if (link->left != NULL)
                link->left->right = link;
            link->right = NULL;
            // same-heap, so need to update thread
            __vnza_format_block (link->inner, size);
            __atomic_store_n (&linkage->head, link, __ATOMIC_RELEASE);
            pthread_mutex_unlock (&linkage->lock);
            object = __vnza_alloc_from_block (link->inner);
            return object;
        } else {
            pthread_mutex_unlock (&empty_linkage->lock);

            // pull from global; label probably no longer necessary
            // __pull_from_global:;
            struct __vnza_heap * global_heap             = __vnza_get_global_heap ();
            struct __vnza_linkage * global_sized_linkage = &global_heap->sized_linkages[linkage_index];
            pthread_mutex_lock (&global_sized_linkage->lock);
            if (global_sized_linkage->head != NULL) {
                struct __vnza_item * link = global_sized_linkage->head;
                // i don't think we need this...
                // while (link != NULL && (__atomic_load_n (&link->inner->__allocation_count, __ATOMIC_ACQUIRE) * link->inner->__object_size) < 0x1000) {
                // link = link->right;
                // }
                if (link == NULL)
                    goto __pull_from_global_unsized;
                global_sized_linkage->head = link->right;
                if (link->right != NULL)
                    link->right->left = NULL;
                pthread_mutex_unlock (&global_sized_linkage->lock);
                link->left = __atomic_load_n (&linkage->head, __ATOMIC_ACQUIRE);
                if (link->left != NULL)
                    link->left->right = link;
                link->right              = NULL;
                link->inner->__thread_id = __vnza_get_local_thread_id ();
                __atomic_store_n (&linkage->head, link, __ATOMIC_RELEASE);
                pthread_mutex_unlock (&linkage->lock);
                object = __vnza_alloc_from_block (link->inner);
                return object;
            } else {
            __pull_from_global_unsized:
                pthread_mutex_unlock (&global_sized_linkage->lock);
                // aight try the unsized
                struct __vnza_linkage * global_unsized_linkage = &global_heap->empty_linkage;
                pthread_mutex_lock (&global_unsized_linkage->lock);
                if (global_unsized_linkage->head != NULL) {
                    struct __vnza_item * link    = global_unsized_linkage->head;
                    global_unsized_linkage->head = link->right;
                    if (link->right != NULL)
                        link->right->left = NULL;
                    pthread_mutex_unlock (&global_unsized_linkage->lock);
                    // because __vnza_alloc is the only writer, would we be allowed to direct read b/c guarantee of no overlap?
                    link->left = __atomic_load_n (&linkage->head, __ATOMIC_ACQUIRE);
                    if (link->left != NULL)
                        link->left->right = link;
                    link->right = NULL;
                    __vnza_format_block (link->inner, size);
                    link->inner->__thread_id = __vnza_get_local_thread_id ();
                    __atomic_store_n (&linkage->head, link, __ATOMIC_RELEASE);
                    pthread_mutex_unlock (&linkage->lock);
                    object = __vnza_alloc_from_block (link->inner);
                    return object;
                } else {
                    pthread_mutex_unlock (&global_unsized_linkage->lock);
                    // aight gotta do a direct allocation from OS
                    struct __vnza_block * block;
                    if (__vnza_alloc_block (&block, size) != 0) {
                        abort ();
                        return NULL;
                    }
                    struct __vnza_item * item;
                    if (__vnza_create_item (&item, block) != 0) {
                        abort ();
                        return NULL;
                    }
                    item->left = __atomic_load_n (&linkage->head, __ATOMIC_ACQUIRE);
                    if (item->left != NULL)
                        item->left->right = item;
                    item->right = NULL;
                    __atomic_store_n (&linkage->head, item, __ATOMIC_RELEASE);
                    pthread_mutex_unlock (&linkage->lock);
                    object = __vnza_alloc_from_block (item->inner);
                    return object;
                }
            }
        }
    } else {
        __vnza_backup_allocate (size);
    }
}

void __vnza_dealloc_from_block (void * object, struct __vnza_block * block)
{
    if (block->__thread_id == __vnza_get_local_thread_id ()) {
        if (__builtin_expect (block->__freeptr_local == NULL, 0)) {
            *(uint16_t *)object = 0xffff;
        } else {
            *(uint16_t *)object = (block->__freeptr_local - block->__range_begin);
        }
        block->__freeptr_local = (uintptr_t)object;
        __atomic_fetch_sub (&block->__allocation_count, 1, __ATOMIC_ACQ_REL);
    } else {
        pthread_mutex_lock (&block->__freeptr_lock);
        if (__builtin_expect (block->__freeptr_global == NULL, 0)) {
            *(uint16_t *)object = 0xffff;
        } else {
            *(uint16_t *)object = (block->__freeptr_global - block->__range_begin);
        }
        block->__freeptr_global = (uintptr_t)object;
        __atomic_fetch_sub (&block->__allocation_count, 1, __ATOMIC_ACQ_REL);
        pthread_mutex_unlock (&block->__freeptr_lock);
    }

    if (__atomic_load_n (&block->__allocation_count, __ATOMIC_ACQUIRE) == 0) {
        if (__atomic_load_n (&__atomic_load_n (&block->__block_linkage, __ATOMIC_ACQUIRE)->head, __ATOMIC_ACQUIRE) != block) {
            pthread_mutex_lock (&__atomic_load_n (&block->__block_linkage, __ATOMIC_ACQUIRE)->lock);
            if (block->__block_linkage->head != block) {
                struct __vnza_linkage * _bl = block->__block_linkage;
                // find the item and disattach it
                struct __vnza_item * _current = _bl->head->left;
                while (_current != NULL && _current->inner != block)
                    _current = _current->left;
                if (_current == NULL)
                    __builtin_unreachable ();
                if (_current->left != NULL)
                    _current->left->right = _current->right;
                // we know there's one on the right
                _current->right->left = _current->left;
                if (_bl->__empties > 4) {
                    // send it to local unsized or global unsized
                    if (0 != __vnza_add_item_to_linkage (_current, &__vnza_local_heap__.empty_linkage))
                        if (0 != __vnza_add_item_to_linkage (_current, &__vnza_get_global_heap ()->empty_linkage)) {
                            __vnza_free_block (_current->inner);
                            __vnza_free_item (_current);
                        }
                } else {
                    _current->right  = _bl->head->right;
                    _bl->head->right = _current;
                    if (_current->right != NULL)
                        _current->right->left = _current;
                    // we know head is the new left
                    _current->left        = _bl->head;
                    _current->left->right = _current;
                    ++_bl->__empties;
                }
            }
            pthread_mutex_unlock (&block->__block_linkage->lock);
        }
    }
}

void __vnza_dealloc_block (void * object)
{
    struct __vnza_lut * lut      = __vnza_get_lut ();
    uint64_t linkage_index       = __vnza_lut_index (object, lut);
    struct __vnza_item * current = lut->linkages[linkage_index];
    uintptr_t addr               = (uintptr_t)object;
    if (current != NULL) {
        if ((addr - current->inner->__range_begin) < 0x4000ull) {
            __vnza_dealloc_from_block (object, current->inner);
            return;
        } else
            current = current->right;
    } else {
        __vnza_backup_free (object);
    }
}
