#include <iostream>
#include <filesystem>
#include <string>
#include <map>
#include <list>

#include <cstdio>
#include <cstring>
#include <libgen.h>

#include <cassert>
#include <cmath>
#include <unistd.h>
#include <sys/stat.h>
#include <type_traits>

#include "version.h"

namespace fs = std::filesystem;

bool verbose = false;

template <typename T, size_t K> class basic_histogram {
	public:
		using value_type = T;
		constexpr static size_t type_size = sizeof(value_type);
		constexpr static size_t type_bits = type_size * 8;
		constexpr static size_t type_cardinality = (1 << type_bits);
		constexpr static size_t block_nmemb = K;
		constexpr static long block_size = block_nmemb * type_size;
		static_assert(std::is_integral<value_type>::value, "basic_histogram::value_type isn't integral");
		static_assert(std::is_unsigned<value_type>::value, "basic_histogram::value_type isn't unsigned");
	private:
		int subtotal[type_cardinality];
		long total;
		value_type block[block_nmemb];
		size_t block_N;
	public:
		basic_histogram() {
		}
		size_t N() const {
			return block_N;
		}
		void reset() {
			memset(subtotal, 0, sizeof(subtotal));
			memset(block, 0, sizeof(block));
			total = 0;
		}
		void write_block(FILE *fp) const {
			if(fwrite(block, type_size, N(), fp) != N())
				throw std::runtime_error(std::string("fwrite() failed: ") + strerror(errno));
		}
		void build_model(FILE *fp) {
			reset();
			if((block_N = fread(block, type_size, block_nmemb, fp)) != block_nmemb)
				if(not feof(fp))
					throw std::runtime_error(std::string("fread(...) failed: ") + strerror(errno));
			for(size_t n = 0; n < N(); n++)
				see(block[n]);
		}
		void see(value_type x) {
			subtotal[x]++;
			total++;
		}
		double operator*(const basic_histogram& that) const {
			static_assert(std::numeric_limits<value_type>::max() < std::numeric_limits<long>::max(), "problem with numeric limits");
			long product = 0;
			long q1 = 0;
			long q2 = 0;
			for(long x = 0; x <= std::numeric_limits<value_type>::max(); x++)  {
				auto s1 = subtotal[x];
				auto s2 = that.subtotal[x];
				product += s1 * s2;
				q1 += s1 * s1;
				q2 += s2 * s2;
			}
			return (double)product / (sqrt(q1) * sqrt(q2));
		}
};

template <size_t K> using histogram = basic_histogram<uint8_t,K>;

static_assert(std::is_trivially_copyable<histogram<1>>::value, "basic_histogram isn't trivially copyable");

template <size_t K> void abridge(FILE *fp, FILE *gp, double threshold) {

	long sz;
	long pos;
	long dpos;

	auto msg = [&pos, &dpos]() -> auto& { return (std::cerr << '[' << pos << '+' << dpos << "] "); };

	histogram<K> local_model;
	histogram<K> trial_model;

	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	pos = ftell(fp);
	dpos = 0;

	msg() << "building model at " << ftell(fp) << std::endl;
	local_model.build_model(fp);
	msg() << "writing block with " << local_model.N() << " elements" << std::endl;
	local_model.write_block(gp);

	pos = ftell(fp);

	msg() << "starting position" << std::endl;

	while(pos < sz) {
		for(int delta = 0; delta < 14; delta++) {

			dpos = ((1 << delta) - 1) * histogram<K>::block_size;

			long nextpos = std::min(pos+dpos, std::max(pos, sz-histogram<K>::block_size));

			if(fseek(fp, nextpos, SEEK_SET) == -1)
				throw std::runtime_error(std::string("fseek(...) failed: ") + strerror(errno));

			msg() << "building model at " << ftell(fp) << '/' << sz << std::endl;

			trial_model.build_model(fp);

			if(feof(fp) or ftell(fp) == sz) {
				msg() << "breaking" << (feof(fp) ? "" : " perfectly") <<  ", eof after model" << std::endl;
				break;
			}

			double score = local_model * trial_model;

			msg() << "score := " << score << std::endl;
			if(score < threshold) {
				msg() << "breaking, score < " << threshold << std::endl;
				break;
			}
		}

		pos = ftell(fp);
		msg() << "promoting trial model to local model" << std::endl;
		local_model = trial_model;
		msg() << "writing block with " << local_model.N() << " elements" << std::endl;
		local_model.write_block(gp);
	}

	fclose(gp);
}

template <size_t K> void abridge(const fs::path& p, double threshold) {
	FILE *fp = fopen(p.c_str(), "r");
	if(fp == nullptr)
		throw std::runtime_error(std::string("failed to open ") + p.string());
	abridge<K>(fp, stdout, threshold);
	fclose(fp);
}

const char * version() {
	static char string[80];
	snprintf(string, sizeof(string), "PMUT/%s version %s", "pcopy", PMUTVERSION);
	return string;
}

struct option {
	const char *flag;
	const char *desc;
};

std::list<option> options = {
	{ "v", "verbose" },
	{ "V", "version" },
	{ "h", "help" },
	{ "b:", "blocksize -- default: 16384, valid: 256, 16384, 65536" },
	{ "s:", "score -- default: 0.90" }
};

void usage(char *prog) {
	std::cerr << std::endl << "usage: " << basename(prog) << " [options] filename" << std::endl << std::endl;
	for(auto o : options)
		std::cerr << '\t' << '-' << o.flag[0] << (o.flag[1] == ':' ? " ARG" : "    ") << '\t' << o.desc << std::endl;
	std::cerr << std::endl;
	std::cerr << version() << std::endl;
	std::cerr << "report bugs to <aempirei@gmail.com>" << std::endl << std::endl;
}

int main(int argc,char **argv) { 

	double score = 0.9;
	unsigned int blocksize = 16384;

	auto program = argv[0];
	std::string flags = "";

	for(auto o : options)
		flags += o.flag;

	int opt;

    while ((opt = getopt(argc, argv, flags.c_str())) != -1) {

        switch (opt) {
			case 's':
				score = strtod(optarg, nullptr);
				break;
			case 'b':
				blocksize = strtoul(optarg, NULL, 0);
				break;
			case 'V':
				std::cerr << version() << std::endl;
				exit(EXIT_SUCCESS);
			case 'v':
				verbose = !verbose;
				break;
			case 'h':
				usage(program);
				exit(EXIT_SUCCESS);
        }
    }

	if(optind >= argc) {
		usage(program);
		exit(EXIT_FAILURE);


	}
	if(not verbose)
		fclose(stderr);

	std::cerr << "score threshold = " << score << std::endl;
	std::cerr << "block size = " << blocksize << std::endl;

	auto filename = argv[optind];

	if(blocksize == 256)
		abridge<256>(filename, score);
	else if(blocksize == 16384)
		abridge<16384>(filename, score);
	else if(blocksize == 65536)
		abridge<65536>(filename, score);
	else
		throw std::range_error("unsupported block size of " + std::to_string(blocksize));

	return 0;
}
