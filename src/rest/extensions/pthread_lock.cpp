#include "rest/extensions/pthread_lock.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include "pthread.h"

pthread_mutex_t            otLock;
pthread_cond_t             cond               = PTHREAD_COND_INITIALIZER;
static otCommissionerState commissionerResult = OT_COMMISSIONER_STATE_DISABLED;

uint8_t openthread_lock_acquire(LockType lock_type, long timeout_ms)
{
    struct timespec timeout = { 0, 0 };

    switch (lock_type)
    {
    case LOCK_TYPE_BLOCKING:
        // Blocking lock
        return pthread_mutex_lock(&otLock);
        break;

    case LOCK_TYPE_NONBLOCKING:
        // Non-blocking lock
        return pthread_mutex_trylock(&otLock);
        break;

    case LOCK_TYPE_TIMED:
        // Timed lock
        clock_gettime(CLOCK_REALTIME, &timeout); // Get current time
        timeout.tv_nsec += (timeout_ms * 1000);
        // Ensure that tv_nsec doesn't exceed 1 second (1e9 nanoseconds).
        if (timeout.tv_nsec >= 1000000000)
        {
            timeout.tv_sec += timeout.tv_nsec / 1000000000;
            timeout.tv_nsec %= 1000000000;
        }
        return pthread_mutex_timedlock(&otLock, &timeout);
        break;

    default:
        return -1;
    }

    return -1;
}

uint8_t openthread_lock_release(void)
{
    return pthread_mutex_unlock(&otLock);
}

uint8_t openthread_lock_init(void)
{
    return pthread_mutex_init(&otLock, NULL);
}

uint8_t openthread_lock_deinit(void)
{
   return pthread_mutex_destroy(&otLock);
}

uint8_t openthread_cond_time_lock(struct timespec *sec)
{
   return pthread_cond_timedwait(&cond, &otLock, sec);
}

void update_commissioner_state(otCommissionerState aState)
{
    openthread_lock_acquire(LOCK_TYPE_BLOCKING, 0);
    commissionerResult = aState;
    openthread_lock_release();
}

otCommissionerState get_commissioner_state()
{
    return commissionerResult;
}

#ifdef __cplusplus
}
#endif
