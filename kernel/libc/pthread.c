#include "pthread.h"

#include <thread/scheduler.h>

#include <string.h>

static int m_pthread_unique_name_gen = 0;

int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *),
                   void *restrict arg) {
    // create the thread
    thread_t* new_thread = create_thread((void*)start_routine, arg, "pthread-%d", m_pthread_unique_name_gen++);
    if (new_thread == NULL) {
        return -1;
    }

    // ready the thread
    scheduler_ready_thread(new_thread);

    // give it out
    *thread = new_thread;

    return 0;
}

int pthread_join(pthread_t thread, void **retval) {
    ASSERT(!"pthread_join: not implemented");
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr) {
    memset(mutex, 0, sizeof(pthread_mutex_t));
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    mutex_lock(mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    mutex_unlock(mutex);
    return 0;
}

int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr) {
    memset(cond, 0, sizeof(pthread_cond_t));
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    return 0;
}

int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex) {
    conditional_wait(cond, mutex);
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    conditional_signal(cond);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    conditional_broadcast(cond);
    return 0;
}
