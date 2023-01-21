#include "rwmutex.h"

void rwmutex_read_lock(rwmutex_t* rwmutex) {
    mutex_lock(&rwmutex->mutex);
    while (rwmutex->is_write_locked || rwmutex->num_waiting_writers) {
        condition_wait(&rwmutex->condition, &rwmutex->mutex, -1);
    }
    rwmutex->num_readers++;
    mutex_unlock(&rwmutex->mutex);
}

void rwmutex_read_unlock(rwmutex_t* rwmutex) {
    mutex_lock(&rwmutex->mutex);
    rwmutex->num_readers--;
    if (rwmutex->num_readers == 0) {
        condition_notify_all(&rwmutex->condition);
    }
    mutex_unlock(&rwmutex->mutex);
}

void rwmutex_write_lock(rwmutex_t* rwmutex) {
    mutex_lock(&rwmutex->mutex);
    while (rwmutex->is_write_locked || rwmutex->num_readers) {
        rwmutex->num_waiting_writers++;
        condition_wait(&rwmutex->condition, &rwmutex->mutex, -1);
        rwmutex->num_waiting_writers--;
    }
    rwmutex->is_write_locked = true;
    mutex_unlock(&rwmutex->mutex);
}

void rwmutex_write_unlock(rwmutex_t* rwmutex) {
    mutex_lock(&rwmutex->mutex);
    rwmutex->is_write_locked = false;
    condition_notify_all(&rwmutex->condition);
    mutex_unlock(&rwmutex->mutex);
}
