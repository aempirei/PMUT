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
#include <atomic>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <climits>
#include <csignal>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <libgen.h>

namespace config {
		bool sync = false;
		bool file = false;
		bool direct = false;
		bool verbose = false;
		bool normal = true;
		std::atomic_bool resize = true;
		std::atomic_bool goodbye = false;
		unsigned int columns;
}

namespace ansi {
#define ESC "\033"
#define CSI ESC "["
#define XTERM CSI "?"
#define ON "h"
#define OFF "l"
	const char *up = CSI "A";
	const char *clreol = CSI "K";
	const char *stopos = CSI "s";
	const char *rclpos = CSI "u";
	const char *show = XTERM "25" ON;
	const char *hide = XTERM "25" OFF;
#undef ESC
#undef CSI
#undef XTERM
#undef ON
#undef OFF
}

struct state;
void print_bar(size_t,size_t,size_t,time_t,size_t);
void print_bar0(const state&);

unsigned int columns() {
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	return w.ws_col;
}

void install_signal_handlers() {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = [](int sig) { config::resize = true; };

    if (sigaction(SIGWINCH, &sa, nullptr) == -1)
	    throw "sigaction failed to set SIGWINCH callback";


    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = [](int sig) { config::goodbye = true; };


    if (sigaction(SIGINT, &sa, nullptr) == -1)
	    throw "sigaction failed to set SIGINT callback";
}

std::string bytestr(size_t sz) {

	std::stringstream ss;
	constexpr int base = 1024;
	constexpr char unit = 'B';
	const char *pre = " kMGTPEZY";

	if(sz >= base) {
		double y = floor(log((double)sz) / log((double)base));
		double d = (double)sz / pow((double)base, y);
		ss << std::setprecision(2) << std::fixed << d;
		if(static_cast<int>(y) > 0)
			ss << pre[static_cast<int>(y)];
		ss << unit;
	} else {
		ss << sz << " byte";
		if(sz > 1)
			ss << 's';
	}

	return ss.str();
}

struct state {
	size_t n = 0;
	size_t N = 0;
	size_t size = 0;
	size_t pos = 0;
	time_t dt = 0;
};

void print_bar(size_t n, size_t N, size_t size, time_t dt, size_t chunk_size) {

	static state z0;

	state z { .n = n, .N = N, .size = size, .pos = size * n / N, .dt = dt };
	
	if(z.dt > z0.dt or z.n - z0.n >= chunk_size)
		print_bar0(z);

	z0 = z;
}

void print_bar0(const state& z) {

	const auto& n = z.n;
	const auto& N = z.N;
	const auto& size = z.size;
	const auto& pos = z.pos;
	const auto& dt = z.dt;

	unsigned int dn = n / dt;

	long bytes_left = N - n;
	long approx_secs_left = bytes_left * dt / n;

	std::cout << '[';

	if(size > 3) {

		auto k = 0U;
		while(k++ < pos)
			std::cout.put('#');
		while(k++ < size)
			std::cout.put('.');

	} else if(n == N) {
		std::cout << "EOF";
	} else {
		std::cout << std::setw(3) << std::dec << std::setfill('0') << (100 * n / N) << '%';
	}

	std::cout << ']' << ansi::clreol << std::endl;

	auto ss = approx_secs_left % 60;
	auto mm = (approx_secs_left / 60) % 60;
	auto hh = approx_secs_left / 3600;

	std::cout << hh << "h " << mm << "m " << ss << "s remaining @ " << bytestr(dn) << "/sec" << ansi::clreol << std::endl;
}

bool write2(int fd, const void *buf, size_t buf_sz) {

	auto left = buf_sz;
	auto p = static_cast<const char *>(buf);

	while(left) {
		auto n = write(fd, p, left);

		if(n == -1) {
			if(errno == EINTR)
				continue;
			return false;
		}

		left -= n;
		p += n;
	}

	return true;
}

bool copy_block(int src, int dst, size_t block_sz, size_t chunk_sz) {

		char *chunk = new char[chunk_sz];

		size_t num_reads = 0;

		size_t left, done;
		time_t start, dt;

		bool success = false;

		start = std::time(nullptr);

		done = 0;
		left = block_sz;

		while(left and not config::goodbye) {

			if(config::resize) {
				config::columns = columns();
				config::resize = false;
			}

			auto n = read(src, chunk, chunk_sz);
			if(n == -1) {
				if(errno == EINTR)
					continue;
				goto cleanup;
			}

			num_reads++;
			
				if(not write2(dst, chunk, n))
					goto cleanup;

			left -= n;
			done += n;

			dt = std::time(nullptr) - start;

			std::cout << ansi::rclpos << std::endl;

			print_bar(done, block_sz, config::columns - 20, dt, chunk_sz);
		}

		success = true;

cleanup:

		delete[] chunk;

		return success;
}

bool copy_file(const char *src, const char *dst) {

		const int chunk_sz = 1 << 16;

		struct stat st;
		int fdsrc = -1;
		int fddst = -1;
		bool success = false;

		std::cout << ansi::hide << '\r' << ansi::clreol << std::flush;

		fdsrc = open(src, O_RDONLY);
		if(fdsrc == -1)
				goto cleanup;

		if(fstat(fdsrc, &st) == -1)
				goto cleanup;

		fddst = open(dst, O_WRONLY | O_CREAT | O_TRUNC | (config::direct ? O_DIRECT : 0) | (config::sync ? config::file ? O_SYNC : O_DSYNC : 0), st.st_mode & 0777);
		if(fddst == -1)
				goto cleanup;

		if(fchmod(fddst, st.st_mode & 07777) == -1)
				goto cleanup;

		std::cout << src << " -> " << dst << " :: ";

		if(config::normal) {
			std::cout << "normal ";
		} else {
			if(config::sync or config::direct or config::file) {
				if(config::sync) std::cout << "synchronous ";
				if(config::direct) std::cout << "direct io ";
				if(config::file) std::cout << "file integrity ";
			}
		}

		std::cout << "mode" << std::endl;

		std::cout << ansi::stopos << std::endl;

		success = copy_block(fdsrc, fddst, st.st_size, chunk_sz);

cleanup:

		auto err = errno;

		std::cout << ansi::rclpos << " : " << (success ? "completed" : "FAILED") << ansi::clreol << ansi::show << std::endl;

		if(fdsrc >= 0)
			close(fdsrc);

		if(fddst >= 0) {
			close(fddst);
			if(not success)
				unlink(dst);
		}

		errno = err;

		return success;
}

std::string version() {   
	std::stringstream ss;
	ss << "PMUT/" << PROGRAM << " version " << VERSION;
	return ss.str();
}

void help(const char *prog) {
		std::cerr << std::endl << "usage: " << prog << " [option]... source destination" << std::endl << std::endl;
		std::cerr << "\t-h show this help" << std::endl;
		std::cerr << "\t-V show version" << std::endl;
		std::cerr << "\t-f toggle file integrity mode mode (default=" << (config::file ? "enabled" : "disabled") << ")" << std::endl;
		std::cerr << "\t-d toggle direct i/o mode (default=" << (config::direct ? "enabled" : "disabled") << ")" << std::endl;
		std::cerr << "\t-s toggle synchronous mode (default=" << (config::sync ? "enabled" : "disabled") << ")" << std::endl;
		std::cerr << std::endl << version() << std::endl;
		std::cerr << "report bugs to <aempirei@gmail.com>" << std::endl << std::endl;
}

int main(int argc, char **argv) {

	int opt;

	/* process options */

	while ((opt = getopt(argc, argv, "sfdvhV?")) != -1) {

		switch (opt) {

			case 's': config::sync = not config::sync; break;
			case 'f': config::file = not config::file; break;
			case 'd': config::direct = not config::direct; break;

			case 'V':
				  std::cerr << version() << std::endl;
				  std::exit(EXIT_FAILURE);
				  break;
			case 'h':

			default:
				  help(*argv);
				  std::exit(EXIT_FAILURE);
				  break;
		}
	}

	config::normal = not (config::sync or config::direct or config::file);

	if(argc - optind != 2) {
		help(*argv);
		return EXIT_FAILURE;
	}

	install_signal_handlers();

	std::exit(copy_file(argv[optind], argv[optind + 1]) ? EXIT_SUCCESS : EXIT_FAILURE);
}
