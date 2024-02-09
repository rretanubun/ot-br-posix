#ifndef PTHREAD_LOCK_HPP_
#define PTHREAD_LOCK_HPP_

#include "rest/extensions/rest_server_common.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "openthread/commissioner.h"
#include "openthread/instance.h"

uint8_t openthread_lock_init(void);

uint8_t openthread_lock_deinit(void);

uint8_t openthread_lock_acquire(LockType lock_type, long timeout_ms);

uint8_t openthread_lock_release(void);

uint8_t openthread_cond_time_lock(struct timespec *sec);

void update_commissioner_state(otCommissionerState aState);

otCommissionerState get_commissioner_state(void);

#ifdef __cplusplus
}
#endif

#endif
