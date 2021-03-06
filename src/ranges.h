#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

//hahaha just _try_ and stop me posix, the _t is mine!
typedef struct condition_t {
    //'s'ingle
    //'r'ange
    char type;
    uint64_t start;
    uint64_t end;
} condition_t;

typedef struct condition_set_t {
    size_t index;
    size_t size;
    condition_t * conditions;
} condition_set_t;

void init_condition_set(condition_set_t * set);
void close_condition_set(condition_set_t * set);

static inline bool conditions_remaining(const condition_set_t * set) {
    return set->index < set->size;
}

static inline bool line_match(condition_set_t * set, uint64_t line) {
    if (set->index == set->size) {
        return false;
    }

    if (set->conditions[set->index].type == 's') {
        if (set->conditions[set->index].start == line) {
            ++(set->index);
            return true;
        }
        return false;
    }

    if (set->conditions[set->index].start > line) {
        return false;
    }

    if (set->conditions[set->index].end == line) {
        ++(set->index);
    }

    return true;
}

int condition_sort(const void * lhs, const void * rhs);

//'s'ingle
//'l'ist
//'r'ange
//'\0' error
char arg_type(const char * arg);

bool extract_list(const char * arg, condition_set_t * set);

bool extract_range(const char * arg, condition_set_t * set);

void consolidate_conditions(condition_set_t *set);
