#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pthread wrapping around our kernel primitives
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <sync/mutex.h>
#include <sync/conditional.h>

// typedefs
typedef mutex_t pthread_mutex_t;
typedef conditional_t pthread_cond_t;
typedef thread_t* pthread_t;

// dummy
typedef void* pthread_attr_t;
typedef void* pthread_mutexattr_t;
typedef void* pthread_condattr_t;

int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *),
                   void *restrict arg);
int pthread_join(pthread_t thread, void **retval);

int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
