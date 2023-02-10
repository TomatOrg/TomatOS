#include "semaphore.h"

void semaphore_signal(semaphore_t* semaphore) {
    mutex_lock(&semaphore->mutex);
    semaphore->value++;
    condition_notify_one(&semaphore->condition);
    mutex_unlock(&semaphore->mutex);
}

bool semaphore_wait(semaphore_t* semaphore, int64_t timeout) {
    mutex_lock(&semaphore->mutex);

    bool satisfied = true;
    while (semaphore->value == 0) {
        // sleep for it, if true we did not get a timeout
        if (condition_wait(&semaphore->condition, &semaphore->mutex, timeout))
            continue;

        // we got a timeout, one last check and then exit
        satisfied = semaphore->value != 0;
        break;
    }

    if (satisfied)
        --semaphore->value;

    mutex_unlock(&semaphore->mutex);

    return satisfied;
}
