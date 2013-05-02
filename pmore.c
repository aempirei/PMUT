/*
 *
 * POST MODERN UNIX TOOLS
 * pmore
 *
 * ansi-code and other terminal escape sequence safe/preserving
 * more-like command although does not accept pipe-style stdin mode
 *
 * Copyright(c) 2013 by Christopher Abad
 * aempirei@gmail.com
 *
 * this code is licensed under the "don't be a retarded asshole" license.
 * if i don't like how you use this software i can tell you to fuck off
 * and you can't use it, otherwise you can use it.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <libgen.h>
#include "version.h"

size_t scroll_sz = 80 * 12;
struct termios t, s;

void exithandler() {
    tcsetattr(0, TCSANOW, &s);
}

void sighandler(int signo) {

    signal(signo, SIG_IGN);
    exit(EXIT_SUCCESS);
}

const char *version() {
    static char string[80];

    snprintf(string, sizeof(string), "PMUT/%s version %s", "pmore", PMUTVERSION);
    return string;
}

void help(const char *prog) {
    fprintf(stderr, "\nusage: %s [options] filename\n\n", prog);
    fprintf(stderr, "press SPACEBAR or ENTER to scroll forward %d bytes and Q to quit\n\n", (int)scroll_sz);
    fputs("\t-b num   display num bytes at a time (valid for num > 0 obviously)\n", stderr);
    fputs("\t-h       show this help\n", stderr);
    fputs("\t-V       show version\n\n", stderr);
    fprintf(stderr, "%s\n", version());
    fputs("report bugs to <aempirei@gmail.com>\n\n", stderr);
}

int main(int argc, char **argv) {
    int opt;
    size_t n;
    const char *fn;
    FILE *fp;
    char *buffer;

    while ((opt = getopt(argc, argv, "hVb:?")) != -1) {

        switch (opt) {
        case 'V':
            printf("%s\n", version());
            exit(EXIT_SUCCESS);
        case 'b':
            scroll_sz = atoi(optarg);
            break;
        case 'h':
        default:
            help(basename(*argv));
            exit(EXIT_SUCCESS);
        }
    }

    fn = argv[optind];

    if (fn == NULL || scroll_sz < 1) {
        help(basename(*argv));
        exit(EXIT_FAILURE);
    }

    if ((fp = fopen(fn, "r")) == NULL) {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    buffer = malloc(scroll_sz + 1);

    /*
     * save terminal state
     */

    if (tcgetattr(0, &t) == -1) {
        perror("tcgetattr()");
        exit(EXIT_FAILURE);
    }

    s = t;

    /*
     * register the terminal cleanup exit handler
     */

    atexit(exithandler);

    /*
     * turn off echoing
     */

    t.c_lflag &= ~(ECHO | ICANON);
    t.c_cc[VTIME] = 0;
    t.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &t);

    /*
     * the only reason we're here! to get a single keystroke!
     */

    while ((n = fread(buffer, 1, scroll_sz, fp)) != 0 && !feof(fp)) {
        int ch;

        fwrite(buffer, 1, n, stdout);

        do {
            ch = getchar();
            if (ch == 'q' || ch == 'Q') {
                fseek(fp, 0, SEEK_END);
                break;
            }
        } while (ch != ' ' && ch != '\n');
    }

    free(buffer);

    exit(EXIT_SUCCESS);
}
