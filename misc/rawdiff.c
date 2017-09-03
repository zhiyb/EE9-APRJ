#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

static const uint8_t header[6] = {0xde, 0xad, 0xbe, 0xef, 0x02, 0x00};

long readFrame(FILE *f, void *p)
{
	unsigned long len = fread(p, 1u, sizeof(header), f);
	if (len != sizeof(header))
		return -1;
	if (strncmp(p, header, sizeof(header)) != 0)
		return -2;
	len = fread(p, 1u, 2u, f);
	if (len != 2u)
		return -3;
	uint16_t ch = *(uint16_t *)p;
	len = fread(p, 1u, ch, f);
	return len != ch ? -4 : len;
}

int main(int argc, char *argv[])
{
	if (argc != 4)
		return 1;
	int avg = atoi(argv[3]);
	FILE *a = fopen(argv[1], "rb");
	FILE *b = fopen(argv[2], "rb");
	void *pa = malloc(65536);
	void *pb = malloc(65536);
	if (!pa || !pb)
		return 2;
	int ia = avg;
	unsigned long err = 0ul;
	for (unsigned long i = 0;; i++) {
		long ca = readFrame(a, pa);
		long cb = readFrame(b, pb);
		if (ca < 0)
			fprintf(stderr, "File A exit with %ld\n", ca);
		if (cb < 0)
			fprintf(stderr, "File B exit with %ld\n", ca);
		if (ca < 0 || cb < 0)
			break;
		if (ca != cb) {
			err = ca >= cb ? ca - cb : cb - ca;
			ca = ca >= cb ? cb : ca;
		}
		uint8_t *qa = pa, *qb = pb;
		while (ca--)
			if (*qa++ != *qb++)
				err++;
		if (--ia == 0) {
			printf("%lu,%lu\n", i, err);
			err = 0ul;
			ia = avg;
		}
	}
	free(pa);
	free(pb);
	fclose(a);
	fclose(b);
	return 0;
}
