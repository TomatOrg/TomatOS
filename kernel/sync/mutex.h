#pragma once

#include <thread/scheduler.h>
#include "semaphore.h"

#include <stdint.h>

typedef struct mutex {
    _Atomic(int32_t) state;
    semaphore_t semaphore;
#ifdef __DEBUG__
    thread_t* locker;
    const char* locker_file;
    int locker_line;
    const char* locker_function;
#endif
} mutex_t;

#define INIT_MUTEX() ((mutex_t){})

void mutex_lock(mutex_t* mutex);

bool mutex_try_lock(mutex_t* mutex);

bool mutex_is_locked(mutex_t* mutex);

void mutex_unlock(mutex_t* mutex);

#ifdef __DEBUG__

#define mutex_lock(mutex) \
    do { \
        mutex_t* __mutex = mutex; \
        ASSERT(get_current_thread() == NULL || __mutex->locker != get_current_thread(), \
               "recursive mutex locking in thread `%s` at %s (%s:%d)", \
               __mutex->locker->name, __mutex->locker_function, __mutex->locker_file, __mutex->locker_line); \
        mutex_lock(__mutex); \
        __mutex->locker = get_current_thread(); \
        __mutex->locker_file = __FILE_NAME__; \
        __mutex->locker_line = __LINE__; \
        __mutex->locker_function = __FUNCTION__; \
    } while (0)

#define mutex_try_lock(mutex) \
    ({ \
        mutex_t* __mutex = mutex; \
        ASSERT(get_current_thread() == NULL || __mutex->locker != get_current_thread(), \
               "recursive mutex locking in thread `%s` at %s (%s:%d)", \
               __mutex->locker->name, __mutex->locker_function, __mutex->locker_file, __mutex->locker_line); \
        bool __success = mutex_try_lock(__mutex); \
        if (__success) { \
            __mutex->locker = get_current_thread(); \
            __mutex->locker_file = __FILE_NAME__; \
            __mutex->locker_line = __LINE__; \
            __mutex->locker_function = __FUNCTION__; \
        } \
        __success; \
    })

#define mutex_unlock(mutex) \
    do { \
        mutex_t* __mutex = mutex; \
        __mutex->locker = NULL; \
        __mutex->locker_file = __FILE_NAME__; \
        __mutex->locker_line = __LINE__; \
        __mutex->locker_function = __FUNCTION__; \
        mutex_unlock(__mutex); \
    } while (0)

#endif

void mutex_self_test();
