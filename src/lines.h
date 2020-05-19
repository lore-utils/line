#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ranges.h"

/*
 * mmap for large files but fallback to read on small files
 * or failure to stat such as for stdin
 */
void file_get_lines(int fd, condition_set_t * set);
void read_get_lines(int fd, condition_set_t * set);
