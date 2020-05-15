#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static void print_help(void) {
    printf("line - a line util to cleanly and sanely extract line(s)\n"
           "Usage:\n"
           "    line <OPTIONS>\n"
           "Options:\n"
           "    Lorum Ipsum\n"
           "\n"
           "    Dolor Sit Amet\n"
           "\n"
           "    -[-h]elp - this message\n"
           "Notes:\n"
           "    Lorem Ipsum.\n");
}

static void get_line(unsigned int line_num, const char *filename) {
    int fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        return;
    }

    struct stat res;
    if (fstat(fd, &res) == -1) {
        perror("lstat");
        return;
    }

    unsigned char *file_data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);

}

int main(int argc, char **argv) {
    if (argc == 1) {
        print_help();
        exit(EXIT_SUCCESS);
    }

    unsigned int line_num = 0;
    const char *filename = NULL;

    int c;
    while (1) {
        int option_index = 0;

        static struct option long_options[]
            = {{"help", no_argument, 0, 'h'},
               {"lines", required_argument, 0, 'l'},
               {"file", required_argument, 0, 'f'},
               {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "?hl:f:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'l':
                line_num = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'h':
            case '?':
                print_help();
                exit(EXIT_SUCCESS);
        }
    }

    return EXIT_SUCCESS;
}

