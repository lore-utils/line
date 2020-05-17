#include "ranges.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

char arg_type(const char * arg) {
    char type = 's';

    //you have to start with a digit and _technically_ a zero length argument is possible
    if (!isdigit(arg[0]) || !arg[0]) {
        return '\0';
    }

    size_t i = 1;
    for (; arg[i]; ++i) {
        if (!isdigit(arg[i])) {
            switch (arg[i]) {
                case '-':
                    type = 'r';
                    ++i;
                    for (; arg[i] && isdigit(arg[i]); ++i)
                        ; //rip to the end
                    break;
                case ',':
                    type = 'l';
                    ++i;
                    for (; arg[i] && (isdigit(arg[i]) || (arg[i - 1] != ',' && arg[i] == ',')); ++i)
                        ; //rip to the end
                    break;
                default:
                    return '\0';
            }
            break;
        }
    }
    if (!arg[i] && isdigit(arg[i - 1])) {
        return type;
    }
    return '\0';
}

void init_condition_set(condition_set_t * set) {
    set->index = 0;
    set->size = 0;
    //lazy built by the extractors
    set->conditions = NULL;
}

void close_condition_set(condition_set_t * set) {
    if (set->conditions) {
        free(set->conditions);
        set->conditions = NULL;
    }
}

bool line_match(struct condition_set_t * set, uint64_t line) {
    if (set->index >= set->size) {
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

static inline void resize_set(condition_set_t * set, size_t addition) {
    set->size += addition;
    if (set->conditions) {
        condition_t * temp = realloc(set->conditions, sizeof(condition_t) * (set->size));
        if (!temp) {
            perror("realloc");
            exit(1);
        }
        set->conditions = temp;
    } else {
        set->conditions = malloc(sizeof(condition_t) * (set->size));
        if (!set->conditions) {
            perror("malloc");
            exit(1);
        }
    }
}

bool extract_list(const char * arg, condition_set_t * set) {
    //to avoid excessive memory fragmentation prefind the total number of ints in this list
    size_t total = 1;
    for (size_t i = 0; arg[i]; ++i) {
        if (arg[i] == ',') {
            ++total;
        }
    }

    resize_set(set, total);

    char * index = (char *)arg;
    for (size_t i = 0; i < total; ++i) {
        char * endptr = NULL;

        const uint64_t start = strtoull(index, &endptr, 0);
        if (start == 0) {
            fprintf(stderr, "Line numbers must be valid integers\n");
            return 1;
        }
        if ((errno == ERANGE && start == ULLONG_MAX) || errno == EINVAL) {
            fprintf(stderr, "Line numbers must be between 1 and %llu\n", ULLONG_MAX);
            return 1;
        }
        index = endptr + 1;
        set->conditions[set->index].type = 's';
        set->conditions[set->index].start = start;
        ++(set->index);
    }

    return 0;
}

bool extract_range(const char * arg, condition_set_t * set) {
    resize_set(set, 1);

    char * index = (char *)arg;
    char * endptr = NULL;
    const uint64_t start = strtoull(index, &endptr, 0);
    if (start == 0) {
        fprintf(stderr, "Line numbers must be valid integers\n");
        return 1;
    }
    if ((errno == ERANGE && start == ULLONG_MAX) || errno == EINVAL) {
        fprintf(stderr, "Line numbers must be between 1 and %llu\n", ULLONG_MAX);
        return 1;
    }
    index = endptr + 1;

    endptr = NULL;
    const uint64_t end = strtoull(index, &endptr, 0);
    if (end == 0) {
        fprintf(stderr, "Line numbers must be valid integers\n");
        return 1;
    }
    if ((errno == ERANGE && end == ULLONG_MAX) || errno == EINVAL) {
        fprintf(stderr, "Line numbers must be between 1 and %llu\n", ULLONG_MAX);
        return 1;
    }
    if (start > end) {
        fprintf(stderr, "Line range must be valid!\n");
        return 1;
    }

    set->conditions[set->index].type = 'r';
    set->conditions[set->index].start = start;
    set->conditions[set->index].end = end;
    ++(set->index);

    return 0;
}

/*
 * Sorts the list of conditions
 * Conditions are compared as follows:
 * If single, compare start values
 * If range, compare start and end values
 * If mixed, ranges before single
 */
int condition_sort(const void * lhs, const void * rhs) {
    const condition_t l = *(const condition_t *)lhs;
    const condition_t r = *(const condition_t *)rhs;

    if (l.start == r.start) {
        if (l.type == 's' && r.type == 's') {
            return 0;
        } else if (l.type == 'r' && r.type == 'r') {
            if (l.end < r.end) {
                return -1;
            } else if (l.end > r.end) {
                return 1;
            } else {
                return 0;
            }
        } else if (l.type == 'r') {
            return -1;
        } else {
            return 1;
        }
    } else {
        if (l.start < r.start) {
            return -1;
        } else {
            return 1;
        }
    }
}

static inline bool is_duplicate(condition_t *l, condition_t *r) {
    if (l->start == r->start) {
        if (l->type == 's' && r->type == 's') {
            //Two identical singles
            return true;
        } else if (l->type == 'r' && r->type == 'r') {
            //Two cases: 5-10,5-6 and 5-6,5-10
            if (r->end > l->end) {
                //Handles the 5-6,5-10 case
                l->end = r->end;
            }
            //Either way, absorb the smaller range
            return true;
        } else {
            //Ranges come first, and absorb the single
            return true;
        }
    } else if (l->type == 'r' && r->type == 'r') {
        //Handles 2-4,3-7
        if (l->end >= r->start) {
            l->end = r->end;
            return true;
            //Handles 2-4,5-7
        } else if ((l->end + 1) == r->start) {
            l->end = r->end;
            return true;
        }
    } else if (l->type != r->type) {
        if (l->type == 'r') {
            //Range vs single
            if (l->end >= r->start) {
                //Handles 2-10,3
                return true;
            }
        } else {
            //Single vs range
            //Range start is after the single
            //DO NOTHING
        }
    }
    return false;
}

//Same algorithm as C++ std::unique
//May be worth doing a custom mergesort to do it in 1 iteration, plus inlining is nice
void consolidate_conditions(condition_set_t *set) {
    if (set == NULL || set->size <= 1 || set->conditions == NULL) {
        return;
    }

    condition_t *head = set->conditions;
    for (size_t i = 1; i < set->size; ++i) {
        condition_t *iter = &set->conditions[i];
        if (!is_duplicate(head, iter) && ++head != iter) {
            *head = *iter;
        }
    }
    ++head;
    set->size -= (set->size - (head - set->conditions));
}
