//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Alloc/Internal>

#include <Venice/Compiler>
#include <Venice/Platform>
#include <Venice/Features>

using namespace vnz;

#if OS(POSIX)
#    include <unistd.h> /* sysconf */
// #include <errno.h> /* errno */ // todo: uncomment when panic works
#endif

#if OS(LINUX)
#    include <sys/sysinfo.h>
/* OS(LINUX) => OS(POSIX) \therefore OS(LINUX) => <unistd.h> available from earlier decl
#    include <unistd.h>
 */
#endif
#if OS(MACOS)
#    include <sys/sysctl.h>
#endif

#if OS(POSIX)
usize alloc::internal::get_pagesize ()
{
#    if defined(_SC_PAGESIZE)
    long _pagesize = sysconf (_SC_PAGESIZE);
#    elif defined(_SC_PAGE_SIZE)
    long _pagesize = sysconf (_SC_PAGE_SIZE);
#    endif
    if (-1 == _pagesize) {
        // panic("vnz::alloc::internal::get_pagesize() failed with code %i, cannot start allocator", errno);
        return 0;
    }
    return { static_cast<usize::UnderlyingType> (_pagesize) };
}
#endif

#if OS(LINUX)
usize alloc::internal::get_physical_memory_size ()
{
    struct sysinfo _sysinfo;
    if (-1 == sysinfo (&_sysinfo)) {
        return 0;
    }
    return { _sysinfo.totalram };
}
#elif OS(MACOS)
usize alloc::internal::get_physical_memory_size ()
{
    math::_i32 _ctl_spec[2] = { CTL_HW, HW_MEMSIZE };
    isize _ctl_ipr;
    usize _ctl_iprl = sizeof (isize);
    if (-1 == sysctl (_ctl_spec, 2, &_ctl_ipr.inner, &_ctl_iprl.inner, NULL, 0)) {
        return 0;
    }
    return { static_cast<usize::UnderlyingType> (_ctl_ipr) };
}
#endif

#if OS(LINUX)
usize alloc::internal::get_number_of_cores ()
{
#    ifdef _SC_NPROCESSORS_ONLN
    long _nproc = sysconf (_SC_NPROCESSORS_ONLN);
    if (-1 == _nproc) {
        return 0;
    }
    return { _nproc };
#    else
    i32 nprocs     = get_nprocs ();
    return { nprocs };
#    endif
}
#elif OS(MACOS)
usize alloc::internal::get_number_of_cores ()
{
    // @TODO; Figure out what HW_ constant is the one we want here...
    // "hw.logicalcpu" seems like what we want
    // but i've also seen "hw.ncpu", "hw.availcpu", "hw.physicalcpu", and "hw.activecpu"
    // EDIT: as of 2013:
    //  hw.ncpu is deprecated
    //  hw.physicalcpu is # of physical CPUs
    //  hw.logicalcpu is # of logical CPUs (for SMT, look into this)
    //          EDIT2 (hw.logicalcpu): HW_LOGICALCPU isn't defined on my install, so i'm inclined to think that this isn't rly a thing anymore
    //  hw.availcpu is # of online logical CPUs <---- I think we want this one
#    define _kVnzaCPUNumberQueryIdentifier HW_AVAILCPU
    math::_i32 _ctl_spec[2] = { CTL_HW, _kVnzaCPUNumberQueryIdentifier };
    i32 _ctl_ipr;
    usize _ctl_iprl = sizeof (i32);
    if (-1 == sysctl (_ctl_spec, 2, &_ctl_ipr.inner, &_ctl_iprl.inner, NULL, 0)) {
        return 0;
    }
    return { static_cast<usize::UnderlyingType> (_ctl_ipr) };
}
#endif
