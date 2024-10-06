/* Copyright (c) 2024 Rodrigo Arias Mallo <rodarima@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 4

static void
usage(void)
{
	fprintf(stderr, "Usage: xdiff <a> <b>\n");
	exit(1);
}

static uint32_t
read_uint32(uint8_t *buf)
{
	return (uint32_t) buf[0] << (0*8) |
		(uint32_t) buf[1] << (1*8) |
		(uint32_t) buf[2] << (2*8) |
		(uint32_t) buf[3] << (3*8);
}

static void
print_hunk_u32(uint64_t aoff, uint8_t *abuf,
		uint64_t boff, uint8_t *bbuf, int n)
{
	printf("@@ u8,u32 -0x%x,%d +0x%x,%d @@\n", aoff, n/4, boff, n/4);
	uint32_t a = read_uint32(abuf);
	uint32_t b = read_uint32(bbuf);
	printf("- 0x%x # %u\n", a, a);
	printf("+ 0x%x # %u\n", b, b);
}

static void
do_diff(FILE *a, FILE *b)
{
	uint64_t aoff = 0, boff = 0;
	uint8_t abuf[N], bbuf[N];

	while (1) {
		size_t aread = fread(abuf, 1, N, a);
		size_t bread = fread(bbuf, 1, N, b);

		if (ferror(a)) {
			perror("fread a failed");
			exit(1);
		}

		if (ferror(b)) {
			perror("fread b failed");
			exit(1);
		}

		if (feof(a) && feof(b))
			break;

		if (aread != N || bread != N) {
			perror("bad size read");
			exit(1);
		}

		if (memcmp(abuf, bbuf, N) != 0)
			print_hunk_u32(aoff, abuf, boff, bbuf, N);

		aoff += aread;
		boff += bread;
	}
}

int
main(int argc, char *argv[])
{
	if (argc != 3)
		usage();

	const char *a_fpath = argv[1];
	const char *b_fpath = argv[2];

	FILE *af = fopen(a_fpath, "r");
	if (af == NULL) {
		fprintf(stderr, "cannot open %s: %s\n",
				a_fpath, strerror(errno));
		exit(1);
	}

	FILE *bf = fopen(b_fpath, "r");
	if (af == NULL) {
		fprintf(stderr, "cannot open %s: %s\n",
				b_fpath, strerror(errno));
		exit(1);
	}

	printf("--- %s\n", a_fpath);
	printf("+++ %s\n", b_fpath);

	do_diff(af, bf);

	fclose(bf);
	fclose(af);

	return 0;
}
