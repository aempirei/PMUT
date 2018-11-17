#include <unistd.h>
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
	const ssize_t long_num = 1 << 14;
	const ssize_t buf_sz = sizeof(long) * long_num;
	ssize_t data_sz = 1 << 26;
	double rate;
	const char *suffix = "";
	int fd;

	struct timeval tv1, tv2, dtv;

	if(argc > 2)
		data_sz = 1 << strtoul(argv[2], NULL, 0);

	if(data_sz < buf_sz)
		data_sz = buf_sz;

	data_sz -= (data_sz % buf_sz);

	buf = malloc(buf_sz);
	
	srandom(time(NULL));

	for(ssize_t n = 0; n < long_num; n++)
		((long *)buf)[n] = random();
	
	if(argc < 2) {
		fprintf(stderr, "\nusage: %s filename [log2_datasize]\n\n", basename(*argv));
		exit(EXIT_FAILURE);
	}

	printf("datasize = %ld (log2 %.1f)\n", data_sz, log2(data_sz));
	printf("filename = %s\n", argv[1]);

	fd = open(argv[1], O_CREAT | O_SYNC | O_WRONLY, 0600);
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
	unlink(argv[1]);

	exit(EXIT_SUCCESS);
}
