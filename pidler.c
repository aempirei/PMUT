/*
 *
 * POST MODERN UNIX TOOLS
 * pidler
 *
 * this program idles until someone sends SIGINT through CTRL+C or other means.
 *
 * Copyright(c) 2010 by Christopher Abad
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
#include <libgen.h>
#include <ctype.h>

const char *
version()
{
    static char string[80];
    snprintf(string, sizeof(string), "PMUT/%s version %s", PROGRAM, VERSION);
    return string;
}

void
help(const char *prog)
{
    fprintf(stderr, "\nusage: %s [options]\n\n", prog);
    fputs("idle in the foreground until a SIGINT s recieved via CTRL+C\n\n"
          "\t-v verbose. show ticks every 5 seconds\n"
          "\t-h show this help\n" "\t-V show version\n\n", stderr);
    fprintf(stderr, "%s\n", version());
    fputs("report bugs to <aempirei@gmail.com>\n\n", stderr);
}

int
main(int argc, char **argv)
{
    int n;
    int opt;
    int ticks = 0;

    while ((opt = getopt(argc, argv, "vhV?")) != -1) {

        switch (opt) {
        case 'v':
            ticks = 1;
            break;
        case 'V':
            printf("%s\n", version());
            exit(EXIT_SUCCESS);
        case 'h':
        default:
            *argv = basename(*argv);
            help(*argv);
            exit(EXIT_SUCCESS);
        }
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    while (1) {
        n = 5;
        while ((n = sleep(n)))
            perror("sleep()");
        if (ticks)
            putchar('.');
    }

    exit(EXIT_SUCCESS);
}
