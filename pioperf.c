#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/time.h>
#include <math.h>


int writeall(int fd, const void *buf, ssize_t buf_sz) {
	ssize_t done, left;
	done = 0;
	left = buf_sz;
	do {
		ssize_t n = write(fd, (const char *)buf + done, left);
		if(n == -1) {
			if(errno != EINTR)
				return -1;
		} else {
			left -= n;
			done += n;
		}
	} while(left > 0);
	return 0;
}

int main(int argc, char **argv) {
	void *buf;
	void *aligned;
	const ssize_t long_num = 1 << 14;
	const long page_size = sysconf(_SC_PAGESIZE);
	const ssize_t buf_sz = sizeof(long) * long_num;
	ssize_t data_sz = 1 << 26;
	ssize_t misalign;
	double rate;
	const char *suffix = "";
	int fd;
	char mode;
	const char *filename;

	struct timeval tv1, tv2, dtv;

	if(argc > 3)
		data_sz = 1 << strtoul(argv[3], NULL, 0);

	if(data_sz < buf_sz)
		data_sz = buf_sz;

	data_sz -= (data_sz % buf_sz);

	buf = malloc(buf_sz * 2);
	misalign = (ssize_t)buf % buf_sz;
	aligned = buf + buf_sz - misalign;
	
	fprintf(stderr, "page size %ld, buffer size %zd\n", page_size, buf_siz);
	if(misalign)
		fprintf(stderr, "buffer mis-aligned by %d bytes, moved to %p\n", misalign, buf);
	
	srandom(time(NULL));

	for(ssize_t n = 0; n < long_num; n++)
		((long *)buf)[n] = random();
	
	if(argc < 3) {
		fprintf(stderr, "\nusage: %s {d|s|n} filename [log2_datasize]\n\n", basename(*argv));
		exit(EXIT_FAILURE);
	}

	filename = argv[2];
	mode = tolower(*argv[1]);

	printf("datasize = %ld (log2 %.1f)\n", (long)data_sz, log2(data_sz));
	printf("filename = %s\n", filename);
	printf("mode = %s\n", mode == 'd' ? "DSYNC" : mode == 's' ? "SYNC" : "NOSYNC");

	fd = open(filename, O_CREAT | (mode == 'd' ? O_DSYNC : mode == 's' ? O_SYNC : 0) | O_WRONLY, 0600);
	if(fd == -1) {
		perror("open(...)");
		exit(EXIT_FAILURE);
	}

	gettimeofday(&tv1, NULL);

	for(ssize_t left = data_sz; left > 0; left -= buf_sz) {
		if(writeall(fd, buf, buf_sz) == -1) {
			perror("writeall(...)");
			exit(EXIT_FAILURE);
		}
	}

	gettimeofday(&tv2, NULL);

	dtv.tv_sec = tv2.tv_sec - tv1.tv_sec;
	dtv.tv_usec = tv2.tv_usec - tv1.tv_usec;
	if(dtv.tv_usec < 0) {
		dtv.tv_sec -= 1;
		dtv.tv_usec += 1000000;
	}
	rate = dtv.tv_sec + ((double)dtv.tv_usec / 1000000.0);
	rate = (double)data_sz / rate;
	if(rate >= 1 << 30) {
		suffix = "G";
		rate /= (double)(1 << 30);
	} else if(rate >= 1 << 20) {
		suffix = "M";
		rate /= (double)(1 << 20);
	}

	printf("%.3g%sB/sec\n", rate, suffix);

	close(fd);
	unlink(argv[2]);

	exit(EXIT_SUCCESS);
}
