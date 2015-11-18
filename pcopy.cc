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

void print_bar(long a, long b, int x) {

		static int y0 = -1;

		int y = x * a / b;

		if(y != y0) {

				fputs("\033[s", stdout);

				putchar('[');

				for(int i = 0; i < y; i++)
						putchar('#');

				for(int i = y; i < x; i++)
						putchar('.');

				putchar(']');

				fputs("\033[u", stdout);
		}

		y0 = y;
}

int write2(int fd, const void *buf, size_t buf_sz) {

		ssize_t left, done, n;
		const char *p;

		p = (const char *)buf;
		left = buf_sz;
		done = 0;

		while(left > 0) {
				n = write(fd, p + done, left);

				if(n == -1) {
						if(errno == EINTR)
								continue;
						return -1;
				}

				left -= n;
				done += n;
		}

		return 0;
}

int copy_block(int src, int dst, off_t offset, size_t block_sz, size_t chunk_sz) {

		const int bar_sz = 64;

		char *chunk = new char[chunk_sz];

		ssize_t left, done, n;

		bool success = false;

		if(lseek(src, offset, SEEK_SET) == (off_t)-1)
				goto cleanup;

		done = 0;
		left = block_sz;

		while(left > 0) {
				n = read(src, chunk, chunk_sz);
				if(n == -1) {
						if(errno == EINTR)
								continue;
						goto cleanup;
				}

				if(write2(dst, chunk, n) == -1)
						goto cleanup;

				left -= n;
				done += n;

				print_bar(done, block_sz, bar_sz);

		}

		success = true;

cleanup:

		fputs("\033[K", stdout);

		delete[] chunk;

		return success ? 0 : -1;
}

int copy_file(const char *src, const char *dst) {

		const int chunk_sz = 1 << 16;

		struct stat st;
		int fdsrc = -1;
		int fddst = -1;
		bool success = false;

		fputs("\033[?25l", stdout);

		fdsrc = open(src, O_RDONLY);
		if(fdsrc == -1)
				goto cleanup;

		if(fstat(fdsrc, &st) == -1)
				goto cleanup;

		fddst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
		if(fddst == -1)
				goto cleanup;

		if(fchmod(fddst, st.st_mode & 07777) == -1)
				goto cleanup;

		printf("%s -> %s :: ", src, dst);

		if(copy_block(fdsrc, fddst, 0, st.st_size, chunk_sz) == -1)
				goto cleanup;

		success = true;

cleanup:

		int err = errno;

		fputs("\033[?25h", stdout);

		puts(success ? "done" : "error");

		if(fdsrc >= 0)
				close(fdsrc);

		if(fddst >= 0) {
				close(fddst);
				if(not success)
						unlink(dst);
		}

		errno = err;

		return success ? 0 : -1;
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

	if(copy_file(argv[optind], argv[optind + 1]) == -1) {
			perror("copy_file()");
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
