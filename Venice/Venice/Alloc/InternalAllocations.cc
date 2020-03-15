//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Internal>

#include <Venice/Compiler>
#include <Venice/Platform>
#include <Venice/Features>

using namespace vnz;

static const usize _pagesize_cache = alloc::internal::get_pagesize ();

#if OS(POSIX)
#    include <sys/mman.h>
#include <stdlib.h> /* NULL */

#    if defined(MAP_ANON)
static const int _kVnzaAnonymousMappingFlag = MAP_ANON;
#    elif defined(MAP_ANONYMOUS)
static const int _kVnzaAnonymousMappingFlag = MAP_ANONYMOUS;
#    else
// ...
#        error "vnz::alloc (POSIX): needs one of {MAP_ANON, MAP_ANONYMOUS}"
#    endif

void * alloc::internal::allocate_contiguous_pages (usize _number)
{
    void * _pages = mmap (
        NULL,
        _number * _pagesize_cache,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        _kVnzaAnonymousMappingFlag | MAP_SHARED,
        /* Some implementations of mmap require/expect fd==-1 for anonymous maps;
            generally, it's just more compliant. */
        /* Darwin allows certain flags to be set in the fd for (anonymous?) maps (upper 8 bits),
            however, it explicitly allows for fd==-1 to be a valid state with no special meaning. */
        -1,
        /* 0-offset, because there's no file to have an offset in */
        0);
    if (_pages == MAP_FAILED) {
        // errno is set at this point, but we don't really have anything to do with it at this level
        return nullptr;
    }
    return _pages;
}

void alloc::internal::free_contiguous_pages(void * _memory, usize _size) {
    munmap(_memory, _size);
}

#include <stdlib.h>

void * alloc::internal::allocate_contiguous_aligned(usize _size, usize _align)
{
    void * _memory;
    if(0 != posix_memalign(&_memory, _align, _size)) {
        // return value is the error, but there's not much we can do with it at this level
        return nullptr;
    }
    return _memory;
}

void alloc::internal::free_contiguous_memory(void * _memory, /* unused */ usize _size)
{
    free(_memory);
}
#endif
