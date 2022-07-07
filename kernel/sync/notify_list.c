/*
 * Code taken and modified from Go
 *
 * Copyright (c) 2009 The Go Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "notify_list.h"

#include <thread/scheduler.h>

#include <stdatomic.h>

static bool less(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) < 0;
}

uint32_t notify_list_add(notify_list_t* list) {
    return atomic_fetch_add(&list->wait, 1);
}

void notify_list_wait(notify_list_t* list, uint32_t ticket) {
    spinlock_lock(&list->lock);

    // Return right away if this ticket has already been notified.
    if (less(ticket, list->notify)) {
        spinlock_unlock(&list->lock);
        return;
    }

    // Enqueue itself.
    waiting_thread_t* wt = acquire_waiting_thread();
    wt->thread = get_current_thread();
    wt->ticket = ticket;

    if (list->tail == NULL) {
        list->head = wt;
    } else {
        list->tail->next = wt;
    }
    list->tail = wt;

    // mark the lock for unlocking and park
    scheduler_park((void*)spinlock_unlock, &list->lock);

    release_waiting_thread(wt);
}

void notify_list_notify_all(notify_list_t* list) {
    // Fast-path: if there are no new waiters since the last notification
    // we don't need to acquire the lock.
    if (atomic_load(&list->wait) == atomic_load(&list->notify)) {
        return;
    }

    // Pull the list out into a local variable, waiters will be readied
    // outside the lock.
    spinlock_lock(&list->lock);
    waiting_thread_t* wt = list->head;
    list->head = NULL;
    list->tail = NULL;

    // Update the next ticket to be notified. We can set it to the current
    // value of wait because any previous waiters are already in the list
    // or will notice that they have already been notified when trying to
    // add themselves to the list.
    atomic_store(&list->notify, atomic_load(&list->wait));
    spinlock_unlock(&list->lock);

    // Go through the local list and ready all waiters.
    while (wt != NULL) {
        waiting_thread_t* next = wt->next;
        wt->next = NULL;
        scheduler_ready_thread(wt->thread);
        wt = next;
    }
}

void notify_list_notify_one(notify_list_t* list) {
    // Fast-path: if there are no new waiters since the last notification
    // we don't need to acquire the lock.
    if (atomic_load(&list->wait) == atomic_load(&list->notify)) {
        return;
    }

    spinlock_lock(&list->lock);

    // Re-check under the lock if we need to do anything.
    uint32_t ticket = list->notify;
    if (ticket == atomic_load(&list->wait)) {
        spinlock_unlock(&list->lock);
        return;
    }

    // Update the next notify ticket number.
    atomic_store(&list->notify, ticket + 1);

    // Try to find the thread that needs to be notified.
    // If it hasn't made it to the list yet we won't find it,
    // but it won't park itself once it sees the new notify number.
    //
    // This scan looks linear but essentially always stops quickly.
    // Because thread's queue separately from taking numbers,
    // there may be minor reordering in the list, but we
    // expect the thread we're looking for to be near the front.
    // The thread has others in front of it on the list only to the
    // extent that it lost the race, so the iteration will not
    // be too long. This applies even when the g is missing:
    // it hasn't yet gotten to sleep and has lost the race to
    // the (few) other thread's that we find on the list.
    waiting_thread_t* prev = NULL;
    for (waiting_thread_t* wt = list->head; wt != NULL; prev = wt, wt = wt->next) {
        if (wt->ticket == ticket) {
            waiting_thread_t* next = wt->next;
            if (prev != NULL) {
                prev->next = next;
            } else {
                list->head = next;
            }
            if (next == NULL) {
                list->tail = prev;
            }
            spinlock_unlock(&list->lock);
            wt->next = NULL;
            scheduler_ready_thread(wt->thread);
            return;
        }
    }
    spinlock_unlock(&list->lock);
}
