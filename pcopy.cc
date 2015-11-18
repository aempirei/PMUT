/*
 *
 * POST MODERN UNIX TOOLS
 * pcopy
 *
 * copy with verbose status indicator.
 *
 * Copyright(c) 2015 by Christopher Abad
 * aempirei@gmail.com
 *
 * this code is licensed under the "don't be a retarded asshole" license.
 * if i don't like how you use this software i can tell you to fuck off
 * and you can't use it, otherwise you can use it.
 *
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>

#include "version.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

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
		snprintf(buf, sizeof(buf), "%0.2f%c%s", d, pre[(int)y], unit);
	}

	return buf;
}

const char *filesize(const char *fn) {
	struct stat st;
	if(stat(fn, &st) == -1)
		return NULL;
	return bytestr(st.st_size);
}

void copy_file(const char *src, const char *dst) {

	struct stat st;
	int fdsrc, fddst;

	fdsrc = open(src, O_RDONLY);
	if(fdsrc == -1) {
		emsg("open", src);
	}

	if(fstat(fdsrc, &st) == -1) {
		emsg("fstat", src);
		close(fdsrc);
	}

	fddst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
	if(fddst == -1) {
			emsg("open", dst);
	}

	if(fchmod(fddst, st.st_mode & 07777) == -1) {
			emsg("fchmod", dst);
	}

	close(fddst);
	close(fdsrc);
}

const char * version() {   
	static char string[80];
	snprintf(string, sizeof(string), "PMUT/%s version %s", "pcopy", PMUTVERSION);
	return string;
}

void help(const char *prog) {
	char *base = strdup(prog);
	fprintf(stderr, "\nusage: %s [option]... source destination\n\n", basename(base));
	fprintf(stderr, "\t-h show this help\n");
	fprintf(stderr, "\t-V show version\n\n");
	fprintf(stderr, "%s\n", version());
	fprintf(stderr, "%s\n\n", "report bugs to <aempirei@gmail.com>");
	free(base);
}

int main(int argc, char **argv) {

	int opt;

	srand(time(NULL) + getpid());

	setvbuf(stdout, NULL, _IONBF, 0);

	/* process options */

	while ((opt = getopt(argc, argv, "hV?")) != -1) {

		switch (opt) {

			case 'V':

				printf("%s\n", version());
				return EXIT_FAILURE;

			case 'h':
			default:

				help(*argv);
				return EXIT_FAILURE;
		}
	}

	if(argc - optind != 2) {
		help(*argv);
		return EXIT_FAILURE;
	}

	copy_file(argv[optind], argv[optind + 1]);

	return EXIT_SUCCESS;
}
