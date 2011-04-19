/*
 * Toilet File Wiper 1.0 (toilet-1.0)
 * Copyright(c) 2011 by Christopher Abad <aempirei@gmail.com>
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
#define VERSION "1.0"
#define PROGRAM "toilet"
#define NAME "Toilet File Wiper"
#define AUTHOR "Christopher Abad <aempirei@gmail.com>"

enum PATTERN { pzero = 0, pone = 1, prandom, pchargen, pmin = pzero, pmax = pchargen };

/* return pattern in sz bytes, must be de-allocated by free() */

typedef char pattern_fn_t(void);

char pzerof(void) {
	return '\0';
}

char ponef(void) {
	return (char)~0;
}

char prandomf(void) {
	return random();
}

char pchargenf(void) {
	static char ch = '\0';
	return (ch++ & 0077) + 0100;
}

int emsgi(const char *func, long n) {
	int en = errno;
	fprintf(stderr, "%s(%ld): %s\n", func, n, strerror(errno));
	return en;
}

int emsg(const char *func, const char *str) {
	int en = errno;
	fprintf(stderr, "%s(%s): %s\n", func, str, strerror(errno));
	return en;
}

pattern_fn_t *pattern_functions[] = { pzerof, ponef, prandomf, pchargenf };

const char *pattern_names[] = { "0's", "1's", "random", "chargen" };

pattern_fn_t *get_pattern_function(enum PATTERN pattern) {
	return (pattern < pzero || pattern > pchargen) ? NULL : pattern_functions[pattern];
}

const char *bytestr(size_t sz) {

	static const int base = 1024;
	static const char *unit = "B";
	static const char *pre = " kMGTPEZY";
	static char buf[32];

	if(sz == 1) {
		snprintf(buf, sizeof(buf), "%ld byte", (long)sz);
	} else if(sz < base) {
		snprintf(buf, sizeof(buf), "%ld bytes", (long)sz);
	} else {
		double y = floor(log((double)sz) / log((double)base));
		double d = (double)sz / pow((double)base, y);
		snprintf(buf, sizeof(buf), "%0.2lf%c%s", d, pre[(int)y], unit);
	}

	return buf;
}

void *get_pattern(enum PATTERN pattern, size_t sz) {

	void *p = malloc(sz);
	char *s = (char *)p;
	char *e = s + sz;

	pattern_fn_t *pf = get_pattern_function(pattern);

	if(p == NULL) {
		emsgi("malloc", sz);
		return NULL;
	}

	if(pf == NULL) {
		fprintf(stderr, "get_pattern_function(%d)\n", pattern);
		return NULL;
	}

	while(s < e)
		*s++ = (*pf)();

	return p;
}

void scp() {
	fputs("\033[s", stdout);
}

void rcp() {
	fputs("\033[u", stdout);
}

void mark(const char *str) {
	fputs(str, stdout);
	scp();
}

int write_block(int fd, size_t sz, const void *block, size_t block_sz) {

	ssize_t left = sz, done = 0;
	int n;

	if(lseek(fd, SEEK_SET, 0) == (off_t)-1) {
		emsgi("lseek", fd);
		return 0;
	}

	scp();

	while(left > 0) {

		size_t write_sz = MIN(block_sz, left);

		n = write(fd, block, write_sz);

		if(n == -1) {
			if(errno != EINTR) {
				emsgi("write", fd);
				return 0;
			}
			mark("E ");
		} else {
			left -= n;
			done += n;
			if(n != write_sz)
				mark("X ");
		}

		rcp();

		printf("%0.2f%% ", 100.0 * (double)done / (double)sz);
	}

	return 1;
}

const char *filesize(const char *fn) {
	struct stat st;
	if(stat(fn, &st) == -1)
		return NULL;
	return bytestr(st.st_size);
}

void wipe_file(const char *fn, enum PATTERN pattern) {

	static const size_t block_sz = 1 << 20;
	void *block = get_pattern(pattern, block_sz);
	struct stat st;
	int fd;

	if(block == NULL) {
		fprintf(stderr, "get_pattern(%d,%ld)\n", pattern, (long)block_sz);
	}

	printf("→ %s ", pattern_names[pattern]);
		
	/* open file or error message */

	fd = open(fn, O_WRONLY | O_SYNC);
	if(fd == -1) {
		emsg("open", fn);
	}

	/* fd stat or error message */

	if(fstat(fd, &st) == -1) {
		emsg("fstat", fn);
		close(fd);
	}

	if(write_block(fd, st.st_size, block, block_sz) == 0)
		fputs("(failed) ", stdout);

	free(block);

	close(fd);
}

void showhelp(const char *arg) {
	printf("\nusage: %s [options] files...\n\n", arg);
	printf("\t-h   show this help\n");
	printf("\t-V   show version\n");
	putchar('\n');
}

void showversion() {
	printf("%s %s (%s-%s)\n", NAME, VERSION, PROGRAM, VERSION);
}

int main(int argc, char **argv) {

	int i;
	enum PATTERN pattern;
	int ch;

	srand(time(NULL) + getpid());

	setvbuf(stdout, NULL, _IONBF, 0);

	/* process options */

	opterr = 0;

	while((ch = getopt(argc, argv, "hV")) != -1) {
		switch(ch) {
			case 'V':
				showversion();
				exit(EXIT_SUCCESS);
			case 'h':
				showversion();
				showhelp(*argv);
				exit(EXIT_SUCCESS);
			case '?':
				fprintf(stderr, "uknown option -%c\n", optopt);
				exit(EXIT_FAILURE);
		}
	}

	/* wipe each file with each pattern */

	for (i = optind; i < argc; i++) {

		const char *fn = argv[i];

		const char *szstr = filesize(fn);

		if(szstr == NULL) {
			printf("%s → skipping → %s\n", fn, strerror(errno));
			continue;
		}

		printf("%s → %s ", fn, filesize(fn));

		for(pattern = pmin; pattern <= pmax; pattern++)
			wipe_file(fn, pattern);

		if(unlink(fn) == -1) {
			emsg("unlink", fn);
		} else {
			puts("→ unlinked");
		}
	}	

	exit(EXIT_SUCCESS);
}
