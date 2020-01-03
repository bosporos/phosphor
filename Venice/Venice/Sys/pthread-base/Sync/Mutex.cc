// -*- mode: c++ -*-

//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Sys/pthread-base/Sync/Mutex>
#include <errno.h>

using vnz::sys::sync::Mutex;

Mutex::Mutex ()
#if OS(MACOS)
    : inner
{
    _PTHREAD_MUTEX_SIG_init
}
#endif
{
#if !OS(MACOS)
    inner = PTHREAD_MUTEX_INITIALIZER;
#endif
    pthread_mutexattr_t attr;
    pthread_mutexattr_init (&attr);
    // DEFAULT might get us better in-practice performance...
    // But for now, at least, we prefer the safer PTHREAD_MUTEX_NORMAL which guarantees at the very least deadlock-on-reentry
    // DEFAULT gives UB-on-reentry, UB-on-foreign-unlock, and UB-on-reexit
    // wheras NORMAL gives UB-on-foreign-unlock and UB-on-reexit, which, in actual practice from the safe Sync::Mutex in the main library, will never give you any problems
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_NORMAL);
    pthread_mutex_init (&inner, &attr);
    pthread_mutexattr_destroy (&attr);
}

Mutex::~Mutex ()
{
    pthread_mutex_destroy (&inner);
}

void Mutex::lock ()
{
    // may die on following:
    //      EINVAL priority inversion
    //      EINVAL bad mutex
    //      EDEADLK already owned
    const int r = pthread_mutex_lock (&inner);
    // debug_assert_eq(r, 0, "lock on system mutex failed (pthread)");
}

void Mutex::unlock ()
{
    // may die on following:
    //      EPERM unowned
    //      EINVAL bad mutex
    const int r = pthread_mutex_unlock (&inner);
    // debug_assert_eq(r, 0, "unlock on system mutex failed (pthread)");
}

bool Mutex::try_lock ()
{
    // may die on the following:
    //      EINVAL priority inversion
    //      EINVAL bad mutex
    // special case:
    //      EBUSY waiting on lock
    const int r = pthread_mutex_trylock (&inner);

    // debug_assert ((r == 0 || r == EBUSY), "try_lock on system mutex failed (pthread)");
    return r == 0;
}
