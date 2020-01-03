// -*- mode: c++ -*-

//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Compiler>
#include <Venice/Platform>
#include <Venice/Features>

#include <Venice/Sys/pthread-base/Thread/ID>
#include <Venice/Sys/pthread-base/Pthread>

static ::vnz::math::_u64 $___vnz_thread_id ();
static thread_local ::vnz::math::_u64 $___vnz_thread_id___ = $___vnz_thread_id ();

// ============================================================================
// VISIBLE API
// ============================================================================

using ::vnz::sys::thread::ID;

ID::ID ()
    : inner { $___vnz_thread_id___ }
{}

ID::ID (ID const & id)
    : inner { id.inner }
{}

ID::ID (UNUSED ID && id)
    : inner { $___vnz_thread_id___ }
{}

void ID::bind_to_current ()
{
    this->inner = $___vnz_thread_id___;
}

bool ID::is_current () const
{
    return this->inner == $___vnz_thread_id___;
}

bool ID::operator== (ID const & rhs) const
{
    return this->inner == rhs.inner;
}

// ============================================================================
// HERE BE (SMALL) DRAGONS. MORE LIKE LIZARDS, REALLY.
// ============================================================================

#if OS(LINUX)
#    include <unistd.h>
#    include <sys/syscall.h>
#elif OS(FREEBSD)
#    include <unistd.h>
#    include <pthread_np.h>
#endif

::vnz::math::_u64 $___vnz_thread_id ()
{
    ::vnz::math::_u64 tid_temporary = 0;
#if OS(MACOS)
    UNUSED const int r = pthread_threadid_np (NULL, &tid_temporary);
    // debug_assert_eq(r, 0, "pthread_threadid_np failed");
#elif OS(FREEBSD)
#    if OS(LP64)
    static_assert (sizeof (int) == 4 && sizeof (pid_t) == 4, "LP64 expecting int of i32 as pid_t (pthreads)");

    ::vnz::math::_i32 higher = pthread_getthreadid_np ();
    ::vnz::math::_i32 lower  = getpid ();
    tid_temporary            = lower | higher << 32;
#    else
#        error "vnz::$___vnz_thread_id() only supports LP64 BSD (pthreads)"
#    endif
#elif OS(LINUX)
    // In linux, pid_t seems to be equal to __kernel_pid_t, which is an `int`
#    if OS(LP64)
    // In LP64, `int` = i32
    // Happy side effect: we can pack a pair of these into a u64
    static_assert (sizeof (int) == 4 && sizeof (pid_t) == 4, "LP64 expecting int of i32 as pid_t (pthreads)");

    // tid/pid combo should be sufficient
    ::vnz::math::_i32 higher = syscall (SYS_gettid);
    ::vnz::math::_i32 lower  = syscall (SYS_getpid);
    tid_temporary            = lower | higher << 32;
#    else
#        error "vnz::$___vnz_thread_id() only supports LP64 Linux (pthreads)"
#    endif
#else
#    error "vnz::$___vnz_thread_id() does not seem to support the target operating system (pthreads)"
#endif

    return tid_temporary;
}
