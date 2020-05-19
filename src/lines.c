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

static inline size_t optimal_buffersize() {
    const long page_size = sysconf(_SC_PAGESIZE);
    //32 pages was best from testing
    const size_t buffer_size = page_size * 32;
    return buffer_size;
}

void file_get_lines(int fd, condition_set_t * set) {
    struct stat res;
    if (fstat(fd, &res) == -1) {
        perror("fstat");
        return;
    }
    const size_t file_size = res.st_size;
    const size_t buffer_size = optimal_buffersize();

    //the cost of mmap for small files is more expensive than just using a read call
    if (file_size <= buffer_size) {
        read_get_lines(fd, set);
        return;
    }

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

    unsigned char * batch_start = NULL;
    size_t batch_len = 0;

    while (conditions_remaining(set)) {
        unsigned char * line_sep = memchr(read_start, '\n', read_len);
        size_t line_len;
        if (line_sep == NULL) {
            //No separator found
            if (line_match(set, line_count)) {
                line_len = read_len;
                if (batch_len) {
                    batch_len += line_len;
                } else {
                    batch_start = read_start;
                    batch_len = line_len;
                }
            } else {
                if (batch_len) {
                    write(STDOUT_FILENO, batch_start, batch_len);
                    batch_len = 0;
                }
            }
            //EOF
            break;
        } else {
            //Line len includes the \n
            line_len = (line_sep - read_start) + 1;
            if (line_match(set, line_count)) {
                if (batch_len) {
                    batch_len += line_len;
                } else {
                    batch_start = read_start;
                    batch_len = line_len;
                }
            } else {
                if (batch_len) {
                    write(STDOUT_FILENO, batch_start, batch_len);
                    batch_len = 0;
                }
            }
            ++line_count;
            read_start += line_len;
            read_len -= line_len;
        }
    }
    //conditions went right to the end
    if (batch_len) {
        write(STDOUT_FILENO, batch_start, batch_len);
    }

    munmap(buffer, file_size);
    close(fd);
}

void read_get_lines(int fd, condition_set_t * set) {
    const size_t buffer_size = optimal_buffersize();

    unsigned char * restrict buffer = malloc(buffer_size);

    //dont buffer, we got this
    //worse unless we can batch it for stdin
    //setvbuf(stdin, NULL, _IONBF, 0);
    //setvbuf(stdout, NULL, _IONBF, 0);

    //for some reason the already sequential stdin benifits from this
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    madvise(buffer, buffer_size, MADV_SEQUENTIAL);

    uint64_t line_count = 1;

    unsigned char * read_start = NULL;
    size_t read_len = 0;
    size_t line_len = 0;

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
            /*
             * this is to handle the extremely rare edge case of having a final line that is both a match
             * and not newline terminated. it cannot be handled in the null seperator check in case the
             * program happens to be run in a mode with lots of small inputs as it will trigger the null case
             * repeatedly causing early termination
             */
            if (line_len && line_match(set, line_count)) {
                write(STDOUT_FILENO, read_start, line_len);
            }
            break;
        }
        unsigned char * batch_start = NULL;
        size_t batch_len = 0;

        read_start = buffer;
        read_len = ret;

        while (true) {
            unsigned char * line_sep = memchr(read_start, '\n', read_len);
            if (line_sep == NULL) {
                line_len = read_len;
                if (batch_len) {
                    write(STDOUT_FILENO, batch_start, batch_len);
                }
                break;
            } else {
                //Line len includes the \n
                line_len = (line_sep - read_start) + 1;
                if (line_match(set, line_count)) {
                    if (batch_len) {
                        batch_len += line_len;
                    } else {
                        batch_start = read_start;
                        batch_len = line_len;
                    }
                    if (!conditions_remaining(set)) {
                        if (batch_len) {
                            write(STDOUT_FILENO, batch_start, batch_len);
                        }
                        goto cleanup;
                    }
                } else {
                    if (batch_len) {
                        write(STDOUT_FILENO, batch_start, batch_len);
                        batch_len = 0;
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
