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
   snprintf(string, sizeof(string), "PMUT/%s version %s", PROGRAM, VERSION);
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

const char *mod(unsigned int n) {
	switch(n) {
		case 2: return "SHIFT";
		case 3: return "ALT";
		case 4: return "ALT+SHIFT";
		case 5: return "CTRL";
		case 6: return "CTRL+SHIFT";
		case 7: return "CTRL+ALT";
		case 8: return "CTRL+ALT+SHIFT";
		default: return NULL;
	}
}

const char *esc(char f, int a, char c) {
	if(f != '[' && f != 'O') {
		return NULL;
	} else if(c == '~') switch(a) {
		case 1:return "HOME";
		case 2:return "INSERT";
		case 3:return "DELETE";
		case 4:return "END";
		case 5:return "PAGEUP";
		case 6:return "PAGEDOWN";
		case 11:return "F1";
		case 12:return "F2";
		case 13:return "F3";
		case 14:return "F4";
		case 15:return "F5";
		case 17:return "F6";
		case 18:return "F7";
		case 19:return "F8";
		case 20:return "F9";
		case 21:return "F10";
		case 23:return "F11";
		case 24:return "F12";
	} else switch(c) {
		case 'A':return "UP";
		case 'B':return "DOWN";
		case 'C':return "RIGHT";
		case 'D':return "LEFT";
		case 'H':return "HOME";
		case 'F':return "END";
		case 'P':return "F1";
		case 'Q':return "F2";
		case 'R':return "F3";
		case 'S':return "F4";
		case 'Z': return "SHIFT+TAB";
	}

	return NULL;
}

const char *not_null(const char *a, const char *b) {
	return a == NULL ? b : a;
}

const char *escmap(const char *s) {
   static char escmaps[40];
   unsigned short a, b;
   char f, c;
   const char *e, *m, *r;
   snprintf(escmaps, sizeof(escmaps), "<ESC>%s", s);
   r = escmaps;
   f = *s++;
   if(sscanf(s, "%hu;%hu%c", &a, &b, &c) == 3) {
	   m = mod(b);
	   e = esc(f,a,c);
	   if(m != NULL && e != NULL)
		   snprintf(escmaps, sizeof(escmaps), "%s+%s",m,e);
   } else if(sscanf(s, "%hu%c", &a, &c) == 2) {
	   r = not_null(esc(f,a,c), escmaps);
   } else {
	   c = *s++;
	   r = not_null(esc(f,-1,c), escmaps);
   }
   return r;
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
