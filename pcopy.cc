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
#include <climits>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>

#include "version.h"

namespace config {
		bool sync = false;
		bool verbose = false;
};

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

void print_left(int left) {
	if(left < 0)
		printf(" %s", "remaining time unknown");
	else
		printf(" %dh %02dm %02ds remaining", left / 3600, (left % 3600) / 60, left % 60);
}

void print_rate(long rate) {
	printf(" @ %s/sec", bytestr(rate));
}

void print_bar(long a, long b, int x, long rate) {

		static int y0 = -1;

		int y = x * a / b;

		int left = rate == 0 ? INT_MIN : ((b - a) / rate);

		if(y != y0) {

				fputs("\033[s", stdout);

				putchar('[');

				for(int i = 0; i < y; i++)
						putchar('#');

				for(int i = y; i < x; i++)
						putchar('.');

				putchar(']');

				fputs("\033[K", stdout);

				print_left(left);

				print_rate(rate);

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

		const char *scols = getenv("COLUMNS");
		const long lcols = scols != NULL ? strtol(scols, NULL, 0) : 80;
		const int bar_sz = (lcols > 0 ? lcols : 80) - 16;

		char *chunk = new char[chunk_sz];

		ssize_t left, done, n;
		time_t start, elapsed, dt;
		long rate = 0;

		bool success = false;

		if(lseek(src, offset, SEEK_SET) == (off_t)-1)
				goto cleanup;

		start = time(NULL);
		dt = 0;

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

				elapsed = time(NULL) - start;
				if(elapsed > dt) {
					rate = done / elapsed;
					dt = elapsed;
				}

				print_bar(done, block_sz, bar_sz, rate);

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

		fddst = open(dst, O_WRONLY | O_CREAT | O_TRUNC | (config::sync ? O_SYNC : 0), st.st_mode & 0777);
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
		fprintf(stderr, "\t-V show version\n");
		fprintf(stderr, "\t-v toggle verbose mode (default=%s)\n", config::verbose ? "enabled" : "disabled");
		fprintf(stderr, "\t-s toggle synchronous mode (default=%s)\n\n", config::sync ? "enabled" : "disabled");
		fprintf(stderr, "%s\n", version());
		fprintf(stderr, "%s\n\n", "report bugs to <aempirei@gmail.com>");
		free(base);
}

int main(int argc, char **argv) {

		int opt;

		srand(time(NULL) + getpid());

		setvbuf(stdout, NULL, _IONBF, 0);

		/* process options */

		while ((opt = getopt(argc, argv, "svhV?")) != -1) {

				switch (opt) {

						case 's':

								config::sync = not config::sync;
								break;

						case 'v':

								config::verbose = not config::verbose;
								break;

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

		if(config::verbose) {
				if(config::sync)
						puts("synchronous mode");
		}

		if(copy_file(argv[optind], argv[optind + 1]) == -1) {
				perror("copy_file()");
				return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
}
