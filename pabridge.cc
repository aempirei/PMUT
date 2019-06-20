#include <iostream>
#include <filesystem>
#include <string>
#include <map>

#include <cstdio>
#include <cstring>
#include <libgen.h>

#include <cmath>
#include <unistd.h>
#include <sys/stat.h>
#include <type_traits>

#include "version.h"

namespace fs = std::filesystem;

template <typename T> class basic_histogram {
	public:
		using value_type = T;
		constexpr static size_t type_size = sizeof(value_type);
		constexpr static size_t type_bits = type_size * 8;
		constexpr static size_t type_cardinality = (1 << type_bits);
		constexpr static size_t block_nmemb = 16384;
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
			if(fwrite(block, type_size, block_N, fp) != block_N)
				throw std::runtime_error(std::string("fwrite() failed: ") + strerror(errno));
		}
		void build_model(FILE *fp) {
			reset();
			if((block_N = fread(block, type_size, block_nmemb, fp)) != block_nmemb)
				if(not feof(fp))
					throw std::runtime_error(std::string("fread failed: ") + strerror(errno));
			for(size_t n = 0; n < block_N; n++)
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

using histogram = basic_histogram<uint8_t>;
static_assert(std::is_trivially_copyable<histogram>::value, "histogram isn't trivially copyable");

void abridge(FILE *fp, FILE *gp, double threshold) {
	long sz;
	long pos = 0;
	histogram local_model;
	histogram trial_model;

	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, pos, SEEK_SET);

	std::cerr << "building local model at " << pos << '/' << sz << std::endl;
	local_model.build_model(fp);
	std::cerr << "writing block with " << local_model.N() << " elements" << std::endl;
	local_model.write_block(gp);

	while(pos < sz - histogram::block_size) {

		for(int delta = 0; delta < 14; delta++) {

			long dpos = (1 << delta) * histogram::block_size;
			dpos = std::min(dpos + pos, sz - histogram::block_size);
			if(fseek(fp, dpos, SEEK_SET) == -1)
				throw std::runtime_error(std::string("fseek(fp,...) failed: ") + strerror(errno));
			pos = ftell(fp);
			std::cerr << "building trial model at " << pos << '/' << sz << std::endl;
			trial_model.build_model(fp);
			pos = ftell(fp);
			if(pos == sz) {
				std::cerr << "breaking, eof" << std::endl;
				break;
			}

			double score = local_model * trial_model;

			std::cerr << "score := " << score << std::endl;
			if(score < threshold) {
				std::cerr << "breaking, score < " << threshold << std::endl;
				break;
			}
		}
		pos = ftell(fp);
		std::cerr << "promoting trial model to local model" << std::endl;
		local_model = trial_model;
		std::cerr << "writing block with " << local_model.N() << " elements" << std::endl;
		local_model.write_block(gp);
	}

	fclose(gp);
}

void abridge(const fs::path& p, double threshold) {
	if(fs::is_regular_file(p)) {
		FILE *fp = fopen(p.c_str(), "r");
		if(fp == nullptr)
			throw std::runtime_error(std::string("failed to open ") + p.string());
		abridge(fp, stdout, threshold);
		fclose(fp);
	}
}

const char * version() {
	static char string[80];
	snprintf(string, sizeof(string), "PMUT/%s version %s", "pcopy", PMUTVERSION);
	return string;
}

void usage(char *prog) {
	std::cerr << std::endl << "usage: " << basename(prog) << " [--{help|verbose|version}] filename score" << std::endl << std::endl;
	std::cerr << version() << std::endl;
	std::cerr << "report bugs to <aempirei@gmail.com>" << std::endl << std::endl;
}

int main(int argc,char **argv) { 
	auto program = argv[0];
	int argsat = 1;

	if(argc < 2 or strcmp(argv[argsat], "--help") == 0) {
		usage(program);
		exit(EXIT_FAILURE);
	} else if(strcmp(argv[argsat], "--version") == 0) {
		std::cerr << version() << std::endl;
		exit(EXIT_SUCCESS);
	} else if(strcmp(argv[argsat], "--verbose") == 0) {
		argsat++;
	} else {
		fclose(stderr);
	}

	if(argc - argsat != 2) {
		usage(program);
		exit(EXIT_FAILURE);
	}

	auto filename = argv[argsat++];
	double score = strtod(argv[argsat++], nullptr);

	abridge(filename, score);

	return 0;
}

