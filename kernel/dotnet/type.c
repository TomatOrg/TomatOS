#include "type.h"

#include <mem/malloc.h>
#include <util/string.h>

#include <stdalign.h>

type_t make_array_type(type_t type) {
//    if (type->by_ref_type != NULL) {
//        return type->by_ref_type;
//    }

//    type_t new_type = kmalloc(1, sizeof(struct type));
//    memset(new_type, 0, sizeof(struct type));
//    *new_type = *g_array;
//    new_type->is_array = true;
//    new_type->element_type = type;

//    type->by_ref_type = new_type;
//    return new_type;

    return NULL;
}

type_t make_by_ref_type(type_t type) {
    // fast path
    if (type->array_type != NULL) {
        return type->array_type;
    }

    // slow path when it needs allocation
    spinlock_lock(&type->by_ref_type_lock);
    if (type->array_type != NULL) {
        spinlock_unlock(&type->by_ref_type_lock);
        return type->array_type;
    }

    type_t new_type = malloc(sizeof(struct type));
    memset(new_type, 0, sizeof(struct type));
    new_type->assembly = type->assembly;
    new_type->stack_alignment = sizeof(void*);
    new_type->stack_size = sizeof(void*);
    new_type->managed_alignment = alignof(void*);
    new_type->managed_size = alignof(void*);
    new_type->is_by_ref = true;
    new_type->element_type = type;

    type->array_type = new_type;

    spinlock_unlock(&type->by_ref_type_lock);
    return new_type;
}

type_t make_pointer_type(type_t type) {
    // fast path
    if (type->pointer_type != NULL) {
        return type->pointer_type;
    }

    // slow path when it needs allocation
    spinlock_lock(&type->pointer_type_lock);
    if (type->pointer_type != NULL) {
        spinlock_unlock(&type->pointer_type_lock);
        return type->pointer_type;
    }

    type_t new_type = malloc(sizeof(struct type));
    memset(new_type, 0, sizeof(struct type));
    new_type->assembly = type->assembly;
    new_type->stack_alignment = sizeof(void*);
    new_type->stack_size = sizeof(void*);
    new_type->managed_alignment = alignof(void*);
    new_type->managed_size = alignof(void*);
    new_type->is_pointer = true;
    new_type->element_type = type;

    type->pointer_type = new_type;

    spinlock_unlock(&type->pointer_type_lock);
    return new_type;
}

bool type_is_assignable_from(type_t to, type_t from) {
    return to == from;
}
