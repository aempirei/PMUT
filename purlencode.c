/*
 *
 * POST MODERN UNIX TOOLS
 * purlencode
 *
 * this program takes in anything from standard input and encodes it
 * using url encoding (%XX). this is useful for people who script a
 * lot of automated web tools and things. theres also some options
 * to save space by only url encoding non-printables and other stuff.
 * i don't think this needs to be feature rich, but instead should
 * just do the job.
 *
 * Copyright(c) 2009 by Christopher Abad
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
#include <ctype.h>
#include <libgen.h>

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
   fprintf(stderr, "\nusage: %s [options] < inputfile\n", prog);
   fprintf(stderr, "       %s [options] string...\n\n", prog);
   fputs("URL encode either the standard input or the string arguments placing one encoded space between each string\n", stderr);
   fputs("or you could also just put a single string in single or double quotes like a normal person. any trailing\n", stderr);
   fputs("newlines in the standard input will not be trimmed.\n\n", stderr);
   fputs("\t-a encode everything except alpha-numerics\n", stderr);
   fputs("\t-h show this help\n", stderr);
   fputs("\t-V show version\n\n", stderr);
   fprintf(stderr, "%s\n", version());
   fputs("report bugs to <aempirei@gmail.com>\n\n", stderr);
}

int
isnone(int c)
{
   return 0;
}

typedef int ctypefunc(int c);

int
main(int argc, char **argv, char **envp)
{
   ctypefunc *isplain = isnone;
   int ch;
   int opt;
   char *p;
   int i;

   while ((opt = getopt(argc, argv, "ahV?")) != -1) {

      switch (opt) {
      case 'a':
         isplain = isalnum;
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

   if (optind >= argc) {
      /*
       * if there are no arguments after the options, then use standard input
       */
      while ((ch = getchar()) != EOF) {
         if (isplain(ch))
            putchar(ch);
         else
            printf("%%%02x", ch);
      }
   } else {
      /*
       * if theres arguments after the options, then encode the arguments, putting a single space between each
       */
      for (i = optind; i < argc; i++) {
         for (p = argv[i]; *p; p++) {
            if (isplain(*p))
               putchar(*p);
            else
               printf("%%%02x", *p);
         }
         if (i < argc - 1)
            fputs("%20", stdout);
      }
   }

   putchar('\n');

   exit(EXIT_SUCCESS);
}
