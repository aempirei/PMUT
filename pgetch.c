/*
 *
 * POST MODERN UNIX TOOLS
 * pgetch
 *
 * a program that returns one keystroke, and encodes other interesting
 * non-8-bit-ASCII keystrokes as strings (such as CTRL-C as "INT")
 * the useful this about this application is that unlike normal getchar()
 * and every other application that uses normal term settings is that
 * it does not require an enter key to be pressed to catch standard
 * intput data from the keyboard, which opens up scripting to a whole
 * new paradigm. wow, bob, wow!
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
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include "version.h"

struct termios t, s;

void
exithandler()
{
   tcsetattr(0, TCSANOW, &s);
}

void
sighandler(int signo)
{
   signal(signo, SIG_IGN);
   exit(EXIT_SUCCESS);
}

const char *
version()
{
   static char string[80];
   snprintf(string, sizeof(string), "PMUT/%s version %s", "pgetch", PMUTVERSION);
   return string;
}

void
help(const char *prog)
{
   fprintf(stderr, "\nusage: %s [options]\n\n", prog);
   fputs("input a single character from stdin, returning without waiting for the ENTER key to be pressed.\n", stderr);
   fputs("many keys are mapped to a text string such as 'F1' or 'ENTER' or 'ALT-ENTER' or 'CTRL-C'.\n\n", stderr);
   fputs("\t-h show this help\n", stderr);
   fputs("\t-V show version\n\n", stderr);
   fprintf(stderr, "%s\n", version());
   fputs("report bugs to <aempirei@gmail.com>\n\n", stderr);
}

const char *charmap(int ch) {
   static char charmaps[40];
#define CMAP(a,b) if(ch==(a)) return(b)
   CMAP('\33', "ESC");
   else CMAP('\0', "NUL");
   else CMAP('\a', "BEL");
   else CMAP('\3', "INT");
   else CMAP('\4', "EOF");
   else CMAP('\n', "LF");
   else CMAP('\r', "CR");
   else CMAP('\t', "TAB");
   else CMAP('\b', "BS");
   else CMAP('\177', "DEL");
   else CMAP(' ', "SPACE");
#undef CMAP
   else if (ch < 32) snprintf(charmaps, sizeof(charmaps), "CTRL-%c", 'A' + ch - 1);
   else snprintf(charmaps, sizeof(charmaps), "%c", ch);
   return charmaps;
}

/*
 * escape sequence modifier combinations:
 *
 * 2: SHIFT
 * 3: ALT
 * 4: ALT+SHIFT
 * 5: CTRL
 * 6: CTRL+SHIFT
 * 7: CTRL+ALT
 * 8: CTRL+ALT+SHIFT
 *
 */

const char *escmap(const char *s) {
   static char escmaps[40];
#define SMAP(a,b) if(strcmp(a,s)==0) return(b)
   SMAP("OP","NUMLOCK");
   else SMAP("OH","HOME");
   else SMAP("OF","END");
   else SMAP("[P", "PAUSE");
   else SMAP("[A", "UP");
   else SMAP("[B", "DOWN");
   else SMAP("[C", "RIGHT");
   else SMAP("[D", "LEFT");
   else SMAP("[1~","HOME");
   else SMAP("[2~","INSERT");
   else SMAP("[3~","DELETE");
   else SMAP("[4~","END");
   else SMAP("[5~","PAGEUP");
   else SMAP("[6~","PAGEDOWN");
   else SMAP("[11~","F1");
   else SMAP("[12~","F2");
   else SMAP("[13~","F3");
   else SMAP("[14~","F4");
   else SMAP("[15~","F5");
   else SMAP("[16~","F6");
   else SMAP("[17~","F7");
   else SMAP("[18~","F8");
   else SMAP("[20~","F9");
   else SMAP("[21~","F10");
   else SMAP("[23~","F11");
   else SMAP("[24~","F12");
   else SMAP("[Z","SHIFT-TAB");
#undef SMAP
   else snprintf(escmaps, sizeof(escmaps), "ESC+%s", s);
   return escmaps;
}

int
main(int argc, char **argv, char **envp)
{
   int ch;

   int opt;

   while ((opt = getopt(argc, argv, "hV?")) != -1) {

      switch (opt) {
      case 'V':
         printf("%s\n", version());
         exit(EXIT_SUCCESS);
      case 'h':
      default:
         *argv = basename(*argv);
         help(basename(*argv));
         exit(EXIT_SUCCESS);
      }
   }

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
    * ignore SIGINT because we're gonna capture them
    * and return them to the caller as a string
    */

   signal(SIGINT, SIG_IGN);

   /*
    * turn off canonical mode, echoing and signal capture
    */

   t.c_lflag &= ~(ICANON | ECHO | ISIG);
   t.c_cc[VTIME] = 0;
   t.c_cc[VMIN] = 1;
   tcsetattr(0, TCSANOW, &t);

   /*
    * the only reason we're here! to get a single keystroke!
    */

   ch = getchar();

   /*
    *
    * the reason i didnt put a switch statement here is im sick of looking at them
    *
    */

   if (ch == '\033') {

      /*
       * if an ESC was caught, put terminal mode into VMIN=0 so that if
       * there are no keystrokes waiting in the buffer, then print ESC
       * and exist, otherwise process the escape code
       *
       */

      char code[40];
      char *p;

      t.c_cc[VTIME] = 0;
      t.c_cc[VMIN] = 0;
      tcsetattr(0, TCSANOW, &t);

      p = fgets(code, sizeof(code) - 1, stdin);

      if (p == NULL || strlen(p) == 0)
         fputs("ESC", stdout);
      else if(*p == '[' ||  *p == 'O')
         fputs(escmap(p), stdout);
      else if(*p == '\33')
         printf("ALT-%s", escmap(p+1));
      else
         printf("ALT-%s", charmap(*p));

   } else fputs(charmap(ch), stdout);

   putchar('\n');

   /*
    * that was a lot to do for almost nothing!
    */

   exit(EXIT_SUCCESS);
}
