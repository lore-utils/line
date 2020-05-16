#include "lines.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void file_get_lines(int fd, condition_set_t * set) {
    struct stat res;
    if (fstat(fd, &res) == -1) {
        perror("fstat");
        return;
    }
    const size_t file_size = res.st_size;

    /*
     * We originally implemented this using read() calls reading 32 pages at a time.
     * mmap was significantly faster, but in an unconventional way.
     * The total cycle count significantly increased (10-20% by our tests).
     * But the machines stayed at a higher clock speed, which more than offset the difference.
     *
     * Mmap is broken for regular stdin, but not redirection
     * Either way, we should probably get a separate implemention using read() for STDIN_FILENO
     */
    unsigned char * restrict buffer
        = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);

    madvise(buffer, file_size, MADV_SEQUENTIAL);

    uint64_t line_count = 1;

    unsigned char * read_start = buffer;
    size_t read_len = file_size;

    while (conditions_remaining(set)) {
        unsigned char * line_sep = memchr(read_start, '\n', read_len);
        size_t line_len;
        if (line_sep == NULL) {
            //No separator found
            if (line_match(set, line_count)) {
                line_len = read_len;
                write(STDOUT_FILENO, read_start, line_len);
            }
            //EOF
            break;
        } else {
            //Line len includes the \n
            line_len = (line_sep - read_start) + 1;
            if (line_match(set, line_count)) {
                write(STDOUT_FILENO, read_start, line_len);
            }
            ++line_count;
            read_start += line_len;
            read_len -= line_len;
        }
    }

    munmap(buffer, file_size);
    close(fd);
}

void buff_get_lines(int fd, condition_set_t * set) {
    const long page_size = sysconf(_SC_PAGESIZE);
    //32 pages was best from testing
    const size_t buffer_size = page_size * 32;

    unsigned char * restrict buffer = malloc(buffer_size);

    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    madvise(buffer, buffer_size, MADV_SEQUENTIAL);

    uint64_t line_count = 1;

    while (true) {
        ssize_t ret = read(fd, buffer, buffer_size);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return;
        }
        if (ret == 0) {
            break;
        }
        unsigned char * read_start = buffer;
        size_t read_len = ret;

        while (true) {
            unsigned char * line_sep = memchr(read_start, '\n', read_len);
            size_t line_len;
            if (line_sep == NULL) {
                //No separator found
                if (line_match(set, line_count)) {
                    line_len = read_len;
                    write(STDOUT_FILENO, read_start, line_len);
                    if (!conditions_remaining(set)) {
                        goto cleanup;
                    }
                }
                //Actually continue on the read loop, not the line loop
                break;
            } else {
                //Line len includes the \n
                line_len = (line_sep - read_start) + 1;
                if (line_match(set, line_count)) {
                    write(STDOUT_FILENO, read_start, line_len);
                    if (!conditions_remaining(set)) {
                        goto cleanup;
                    }
                }

                ++line_count;
                read_start += line_len;
                read_len -= line_len;
            }
        }
    }
cleanup:

    free(buffer);
    close(fd);
}
