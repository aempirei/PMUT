/*
 *
 * POST MODERN UNIX TOOLS
 * psync
 *
 * sync stdin to file with verbose status indicator.
 *
 * Copyright(c) 2019 by Christopher Abad
 * aeempirei@gmail.com
 *
 * this code is licensed under the "don't be a retarded asshole" license.
 * if i don't like how you use this software i can tell you to fuck off
 * and you can't use it, otherwise you can use it.
 *ud
 */

#include <numeric>
#include <chrono>
#include <iostream>
#include <string>

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
#include <sys/file.h>
#include <libgen.h>

std::string human(double sz) {
	const char pre[] = " kMGTPEZY";
	constexpr int bits = 10;
	constexpr int base = 1 << bits;
	char buf[64];
	uint8_t index = (uint8_t)floor(log2(sz) / bits);
	snprintf(buf, sizeof(buf), "%.1f", (double)sz / pow(base, index));
	return index > 0 ? std::string(buf) + pre[index] : std::string(buf);
}

std::string rate(size_t sz, const std::chrono::time_point<std::chrono::steady_clock>& start_time) {
	auto end_time = std::chrono::steady_clock::now();
	std::chrono::duration<double> dt = end_time - start_time;
	return human((double)sz / dt.count()) + "b/s";
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
			if(errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		}

		left -= n;
		done += n;
	}

	return 0;
}

namespace state {
	int fd = -1;
	int err = 0;
	bool create;
	bool success = false;
	std::string to;
	unsigned long total;
}

const char *cursor_off = "\033[?25l";
const char *cursor_on = "\033[?25h";
const char *xy_save = "\033[s";
const char *xy_restore = "\033[u";
const char *clear_eol = "\033[K";

void cleanup() {

	if(not state::success) {
		std::cout << "failed";
		if(state::err != 0)
			std::cout << ": " << strerror(state::err);
		std::cout << std::endl;
	}

	if(state::fd >= 0) {
		close(state::fd);
		if(state::create and not state::success) {
			std::cout << "removing created partial file" << std::endl;
			unlink(state::to.c_str());
		}
	}

	std::cout << cursor_on << std::flush;
}

void sync_file(const char *filename) {

	constexpr int buffer_size = 1 << 20;
	char buffer[buffer_size];
	state::to = filename;
	struct stat sb;
	state::create = (stat(state::to.c_str(), &sb) == -1 and errno == ENOENT);

	atexit(cleanup);

	std::cout << cursor_off << std::flush;

	if((state::fd = open(state::to.c_str(), O_WRONLY | O_SYNC | O_CREAT, 0755)) != -1) {

		if(flock(state::fd, LOCK_EX | LOCK_NB) != -1) {

			std::cout << (state::create ? "created" : "opened") << ' ' << state::to << " :: " << std::flush;

			auto start_time = std::chrono::steady_clock::now();

			for(;;) {
				ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
				switch(n) {
					case -1:
						if(errno == EINTR || errno == EAGAIN)
							continue;
						return;
					case 0:
						state::success = true;
						return;
					default:
						if(write2(state::fd, buffer, n) == -1)
							return;
						break;
				}
				state::total += n;
				auto rs = rate(state::total, start_time);
				auto current_time = std::chrono::steady_clock::now();
				char secs[16];
				std::chrono::duration<double> dt = current_time - start_time;
				snprintf(secs, sizeof(secs), "%.1fs", dt.count());
				std::cout << xy_save << "written " << state::total << " bytes (" << human(state::total) << "B)" <<
					" :: data rate " << rs <<
					" :: elapsed time " << secs << clear_eol << xy_restore << xy_save << std::flush;
			}
		}
	}
}

std::string version() {
	return std::string("PMUT/" PROGRAM " version " VERSION);
}

std::string base(const char *s) {
	char *t = strdup(s);
	std::string ds = std::string(basename(t));
	free(t);
	return ds;
}

void help(const char *prog) {
	fprintf(stderr, "\nusage: %s [option]... destination\n\n", base(prog).c_str());
	fprintf(stderr, "\t-h show this help\n");
	fprintf(stderr, "\t-V show version\n");
	fprintf(stderr, "%s\n", version().c_str());
	fprintf(stderr, "%s\n\n", "report bugs to <aempirei@gmail.com>");
}

int main(int argc, char **argv) {

	int opt;

	/* process options */

	while ((opt = getopt(argc, argv, "Vh?")) != -1) {

		switch (opt) {

			case 'V':

				printf("%s\n", version().c_str());
				std::exit(EXIT_SUCCESS);

			case 'h':
			default:

				help(*argv);
				std::exit(EXIT_SUCCESS);
		}
	}

	if(argc - optind != 1) {
		help(*argv);
		std::exit(EXIT_FAILURE);
	}

	sync_file(argv[optind]);

	std::exit(state::success ? EXIT_SUCCESS : EXIT_FAILURE);
}
