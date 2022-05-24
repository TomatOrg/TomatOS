#include "pthread.h"
#include "proc/scheduler.h"

#include <string.h>

int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *),
                   void *restrict arg) {
    TRACE("pthread_create");

    // create the thread
    thread_t* new_thread = create_thread((void*)start_routine, arg, "pthread");
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
    TRACE("pthread_mutex_init");
    memset(mutex, 0, sizeof(pthread_mutex_t));
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    TRACE("pthread_mutex_destroy");
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    TRACE("pthread_mutex_lock");
    mutex_lock(mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    TRACE("pthread_mutex_unlock");
    mutex_unlock(mutex);
    return 0;
}

int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr) {
    TRACE("pthread_cond_init");
    memset(cond, 0, sizeof(pthread_cond_t));
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    TRACE("pthread_cond_destroy");
    return 0;
}

int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex) {
    TRACE("pthread_cond_wait");
    conditional_wait(cond, mutex);
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    TRACE("pthread_cond_signal");
    conditional_signal(cond);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    TRACE("pthread_cond_broadcast");
    conditional_broadcast(cond);
    return 0;
}
