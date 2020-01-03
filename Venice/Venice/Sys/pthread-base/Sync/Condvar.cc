// -*- mode: c++ -*-

//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Sys/pthread-base/Sync/Condvar>

using vnz::sys::sync::Condvar;
using vnz::sys::sync::Mutex;

Condvar::Condvar ()
{
    inner = PTHREAD_COND_INITIALIZER;
}

Condvar::~Condvar ()
{
    UNUSED const int r = pthread_cond_destroy (&this->inner);
    // debug_assert_eq(r, 0, "could not destroy condvar (pthread)");
}

void Condvar::notify_one ()
{
    UNUSED const int r = pthread_cond_signal (&this->inner);
    // debug_assert_eq(r, 0, "could not condvar::notify (one) (pthread)");
}

void Condvar::notify_all ()
{
    UNUSED const int r = pthread_cond_broadcast (&this->inner);
    // debug_assert_eq(r, 0, "could not condvar::notify (all) (pthread)");
}

void Condvar::wait (vnz::sys::sync::Mutex & m)
{
    m.lock ();
    UNUSED const int r = pthread_cond_wait (&this->inner, &m.inner);
    // debug assert_eq(r, 0, "could not wait on condvar (pthread)");
}
