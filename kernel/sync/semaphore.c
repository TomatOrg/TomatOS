#include "semaphore.h"

#include <lib/except.h>

void semaphore_signal(semaphore_t* semaphore) {
    mutex_lock(&semaphore->mutex);
    semaphore->value++;
    condvar_notify_one(&semaphore->condition);
    mutex_unlock(&semaphore->mutex);
}

bool semaphore_wait_until(semaphore_t* semaphore, uint64_t deadline) {
    mutex_lock(&semaphore->mutex);
    while (semaphore->value == 0) {
        if (condvar_wait_until(&semaphore->condition, &semaphore->mutex, deadline)) {
            // timed out, exit early
            mutex_unlock(&semaphore->mutex);
            return false;
        }
    }
    semaphore->value--;
    mutex_unlock(&semaphore->mutex);
    return true;
}

void semaphore_reset(semaphore_t* semaphore) {
    mutex_lock(&semaphore->mutex);
    semaphore->value = 0;
    mutex_unlock(&semaphore->mutex);
}

