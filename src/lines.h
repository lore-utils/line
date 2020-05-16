#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ranges.h"

void file_get_lines(int fd, condition_set_t * set);
