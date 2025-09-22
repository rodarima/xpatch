/* Copyright (c) 2024-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "common.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

struct diff {
	char source[PATH_MAX];
	char target[PATH_MAX];
	char tmpname[PATH_MAX];

	size_t src_cursor;
	size_t tmp_cursor;

	FILE *src;
	FILE *tmp;
};

enum type {
	TYPE_I8 = 0,
	TYPE_I16,
	TYPE_I24,
	TYPE_I32,
	TYPE_I64,
	TYPE_U8,
	TYPE_U16,
	TYPE_U24,
	TYPE_U32,
	TYPE_U64,
	TYPE_F32,
	TYPE_F64,
	TYPE_MAX
};

const char *typename[TYPE_MAX] = {
	[TYPE_I8]  = "i8",
	[TYPE_I16] = "i16",
	[TYPE_I24] = "i24",
	[TYPE_I32] = "i32",
	[TYPE_I64] = "i64",
	[TYPE_U8]  = "u8",
	[TYPE_U16] = "u16",
	[TYPE_U24] = "u24",
	[TYPE_U32] = "u32",
	[TYPE_U64] = "u64",
	[TYPE_F32] = "f32",
	[TYPE_F64] = "f64"
};

const size_t typewidth[TYPE_MAX] = {
	[TYPE_I8]  = 1,
	[TYPE_I16] = 2,
	[TYPE_I24] = 3,
	[TYPE_I32] = 4,
	[TYPE_I64] = 8,
	[TYPE_U8]  = 1,
	[TYPE_U16] = 2,
	[TYPE_U24] = 3,
	[TYPE_U32] = 4,
	[TYPE_U64] = 8,
	[TYPE_F32] = 4,
	[TYPE_F64] = 8
};

struct segment {
	size_t addr;
	size_t size;
};

struct hunk {
	enum type addr_type;
	enum type data_type;
	size_t addr_width;
	size_t data_width;
	struct segment a;
	struct segment b;
};

//static void
//usage(void)
//{
//	fprintf(stderr, "Usage: xpatch < 1.xpatch\n");
//	exit(1);
//}

static int
next_line(char *buf, int n)
{
	if (fgets(buf, n, stdin) == NULL) {
		fprintf(stderr, "cannot read next line\n");
		return -1;
	}

	int len = (int) strlen(buf);
	for (int i = len - 1; i >= 0; i++) {
		if (buf[i] != '\n')
			break;

		buf[i] = '\0';
	}

	return 0;
}

static enum type
parse_type(const char *type)
{
	for (int i = 0; i < TYPE_MAX; i++) {
		if (strcmp(type, typename[i]) == 0)
			return i;
	}

	fprintf(stderr, "bad type: %s\n", type);
	exit(1);
}

static void
parse_segment(struct hunk *hunk, struct segment *seg, char ch, char *line)
{
	if (line[0] != ch) {
		fprintf(stderr, "expecting %c character\n", ch);
		exit(1);
	}

	/* Advance symbol (- or +) */
	line++;
	
	char *saveptr;
	char *addr_str = strtok_r(line, ",", &saveptr);

	errno = 0;    /* To distinguish success/failure after call */
	char *endptr;
	seg->addr = (size_t) strtoll(addr_str, &endptr, 0);

	/* Check for various possible errors. */

	if (errno == ERANGE) {
		perror("strtol");
		exit(1);
	}

	if (endptr == addr_str) {
		fprintf(stderr, "No digits were found\n");
		exit(1);
	}

	fprintf(stderr, "addr=%s parsed=0x%zx\n", addr_str, seg->addr);

	char *size_str = strtok_r(NULL, " ", &saveptr);
	seg->size = (size_t) strtoll(size_str, &endptr, 0);
	if (errno == ERANGE) {
		perror("strtol");
		exit(1);
	}

	if (endptr == size_str) {
		fprintf(stderr, "No digits were found\n");
		exit(1);
	}

	fprintf(stderr, "size=%s parsed=0x%zx\n", size_str, seg->size);

	seg->size *= hunk->data_width;
	seg->addr *= hunk->addr_width;

	info("segment: from %zd with size %zd", seg->addr, seg->size);
}

static int
is_hunk_control_line(const char *line)
{
	/* A hunk control line MUST begin with "@@ " */
	if (strncmp(line, "@@ ", 3) != 0)
		return 0;

	/* And end with " @@" as well */
	int n = (int) strlen(line);
	if (n <= 6)
		return 0;

	const char *end = line + n - 3;

	if (strncmp(end, " @@", 3) != 0)
		return 0;

	/* Looks like a hunk */
	return 1;
}

static int
parse_hunk_control(struct hunk *hunk, char *ctl)
{
	memset(hunk, 0, sizeof(struct hunk));

	if (strncmp(ctl, "@@ ", 3) != 0) {
		err("bad hunk control line: %s", ctl);
		return -1;
	}

	/* Point to type spec */
	char *p = &ctl[3];

	if (*p == '-') {
		err("normal hunk, unsupported");
		return -1;
	}

	/* Extract the whole type spec */
	char *saveptr;
	char *type_spec = strtok_r(p, " ", &saveptr);
	(void) type_spec;

	/* Binary hunk must have address type */
	char *saveptr2;
	char *addr_type_str = strtok_r(p, ",", &saveptr2);
	hunk->addr_type = parse_type(addr_type_str);
	hunk->addr_width = typewidth[hunk->addr_type];

	/* And data type */
	char *data_type_str = strtok_r(NULL, ",", &saveptr2);
	hunk->data_type = parse_type(data_type_str);
	hunk->data_width = typewidth[hunk->data_type];

	/* May also have format type */
	char *format_str = strtok_r(NULL, ",", &saveptr2);
	if (format_str == NULL)
		format_str = "default";

	info("a=%s d=%s f=%s",
			typename[hunk->addr_type],
			typename[hunk->data_type],
			format_str);

	/* Now read deletion operation */
	char *delete = strtok_r(NULL, " ", &saveptr);
	parse_segment(hunk, &hunk->a, '-', delete);
	char *addition = strtok_r(NULL, " ", &saveptr);
	parse_segment(hunk, &hunk->b, '+', addition);

	return 0;
}

static int
copy_until_hunk(struct diff *diff, struct hunk *hunk)
{
	/* Compute the source segment */
	off_t src_start = (off_t) diff->src_cursor;
	off_t src_end = (off_t) hunk->a.addr;
	off_t src_size = src_end - src_start;

	off_t dst_start = (off_t) diff->tmp_cursor;
	off_t dst_end = (off_t) hunk->b.addr;
	off_t dst_size = dst_end - dst_start;

	info("copy from source [%zd, %zd]", src_start, src_end);
	info("into destination [%zd, %zd]", dst_start, dst_end);

	if (src_size != dst_size) {
		err("inter-hunk segment size mismatch");
		return -1;
	}

	char buffer[4096];
	while (1) {
		size_t n = sizeof(buffer);
		if (n > (size_t) src_size)
			n = (size_t) src_size;
		size_t nread = fread(buffer, 1, n, diff->src);
		if (nread == 0) {
			/* FIXME: Might be another error */
			err("source file exhausted");
			return -1;
		}

		size_t nwritten = fwrite(buffer, 1, nread, diff->tmp);
		if (nwritten != nread) {
			err("cannot write the same buffer size");
			return -1;
		}

		info("copied %zd bytes at %zd", nwritten, diff->tmp_cursor);

		src_size -= (off_t) nread;
		diff->src_cursor += nread;
		diff->tmp_cursor += nwritten;

		if (src_size == 0)
			break;
	}

	return 0;
}

static int
is_diff_line(const char *line)
{
	if (strncmp(line, "--- ", 4) != 0)
		return 0;

	return 1;
}

static int
is_hunk_delete_line(const char *line)
{
	/* A hunk delete line MUST begin with "- " */
	if (strncmp(line, "- ", 2) != 0)
		return 0;

	return 1;
}

static int
is_hunk_insert_line(const char *line)
{
	/* A hunk insert line MUST begin with "+ " */
	if (strncmp(line, "+ ", 2) != 0)
		return 0;

	return 1;
}


static int
parse_delete_line(struct diff *diff, struct hunk *hunk, char *line)
{
	(void) diff;
	(void) hunk;

	info("got delete line: %s", line);

	char *values = line + 2;

	/* Only U32 for now, one per line */
	if (hunk->data_type != TYPE_U32) {
		err("unsupported data type: %s", typename[hunk->data_type]);
		return -1;
	}

	char *endptr;
	int32_t v = (int32_t) strtol(values, &endptr, 0);

	/* FIXME: Endianness */
	int32_t src_v;
	fread(&src_v, 4, 1, diff->src);

	info("got value from patch %x, from source %x", v, src_v);

	if (v != src_v) {
		err("mismatch source at address %zx", diff->src_cursor);
		return -1;
	} else {
		info("delete line matches source");
	}

	diff->src_cursor += 4;

	info("advancing src cursor, now at %zd", diff->src_cursor);

	return 0;
}

static int
parse_insert_line(struct diff *diff, struct hunk *hunk, char *line)
{
	(void) diff;
	(void) hunk;
	info("got insert line: %s", line);

	char *values = line + 2;

	/* Only U32 for now, one per line */
	if (hunk->data_type != TYPE_U32) {
		err("unsupported data type: %s", typename[hunk->data_type]);
		return -1;
	}

	char *endptr;
	int32_t v = (int32_t) strtol(values, &endptr, 0);

	info("got value from patch %x", v);

	/* FIXME: Endianness */
	fwrite(&v, 4, 1, diff->tmp);

	diff->tmp_cursor += 4;

	info("advancing tmp cursor, now at %zd", diff->tmp_cursor);

	return 0;
}


static int
apply_delete_lines(struct diff *diff, struct hunk *hunk, char *line)
{
	while (1) {
		if (next_line(line, 4096) != 0) {
			err("EOF when expecting delete line");
			return -1;
		}

		/* Stop on insert line */
		if (is_hunk_insert_line(line))
			break;

		/* Ignore garbarge */
		if (!is_hunk_delete_line(line))
			continue;

		if (parse_delete_line(diff, hunk, line) != 0) {
			err("cannot parse delete line");
			return -1;
		}
	}

	return 0;
}

static int
apply_insert_lines(struct diff *diff, struct hunk *hunk, char *line)
{
	while (1) {
		/* Stop on hunk control line or new diff */
		if (is_hunk_control_line(line) || is_diff_line(line))
			break;

		/* Ignore garbarge */
		if (!is_hunk_insert_line(line))
			continue;

		if (parse_insert_line(diff, hunk, line) != 0) {
			err("cannot parse insert line");
			return -1;
		}

		if (next_line(line, 4096) != 0) {
			/* EOF is fine here */
			break;
		}
	}

	return 0;
}

static int
parse_hunk(struct diff *diff, struct hunk *hunk, char *ctl)
{
	if (parse_hunk_control(hunk, ctl) != 0) {
		err("cannot parse hunk control line: %s", ctl);
		return -1;
	}

	/* Now we need to copy the segment from source as-is in the tmp file, as
	 * it won't be modified by the hunk */

	if (copy_until_hunk(diff, hunk) != 0) {
		err("cannot copy segment until hunk");
		return -1;
	}

	char line[4096];
	if (apply_delete_lines(diff, hunk, line) != 0) {
		err("cannot apply delete lines");
		return -1;
	}
	if (apply_insert_lines(diff, hunk, line) != 0) {
		err("cannot apply insert lines");
		return -1;
	}

	return 0;
}

static int
process_diff(struct diff *diff)
{
	char ctl[4096];

	while (1) {
		if (next_line(ctl, 4096) != 0)
			break;

		/* Ignore garbarge */
		if (is_hunk_control_line(ctl)) {
			struct hunk hunk = { 0 };
			if (parse_hunk(diff, &hunk, ctl) != 0) {
				err("cannot parse hunk");
				return -1;
			}
		}
	}

	return 0;
}

static int
complete_diff(struct diff *diff)
{
	/* Copy the remaining segment at the end of the source to the
	 * destination */

	char buffer[4096];
	while (1) {
		size_t n = sizeof(buffer);
		size_t nread = fread(buffer, 1, n, diff->src);
		if (nread == 0) {
			/* FIXME: Might be another error */
			return 0;
		}

		size_t nwritten = fwrite(buffer, 1, nread, diff->tmp);
		if (nwritten != nread) {
			err("cannot write the same buffer size");
			return -1;
		}

		info("copied %zd bytes at %zd", nwritten, diff->tmp_cursor);

		diff->src_cursor += nread;
		diff->tmp_cursor += nwritten;
	}

	return 0;
}

static int
parse_diff_file(const char *prefix, char *line, char fpath[PATH_MAX])
{
	if (strncmp(line, prefix, strlen(prefix)) != 0) {
		err("expected '%s' line, found: %s", prefix, line);
		return -1;
	}

	char *start = line + strlen(prefix);
	char *tab = strchr(start, '\t');

	/* Accept filenames without extra metadata on the right */
	if (tab != NULL) {
		if (start == tab) {
			err("empty file name: '%s'", line);
			return -1;
		}

		*tab = '\0';
	}

	char *file = start;

	if (snprintf(fpath, PATH_MAX, "%s", file) >= PATH_MAX) {
		err("file name too long: '%s'", file);
		return -1;
	}

	return 0;
}

static int
parse_diff_filenames(struct diff *diff, char *buf)
{
	memset(diff, 0, sizeof(*diff));

	if (parse_diff_file("--- ", buf, diff->source) != 0) {
		err("cannot parse source file name");
		return -1;
	}

	/* The target file may be the same, but we will write to a temporary
	 * file, so no need to open for now */

	char buf2[4096];
	next_line(buf2, 4096);

	if (parse_diff_file("+++ ", buf2, diff->target) != 0) {
		err("cannot parse target file name");
		return -1;
	}

	info("source=%s target=%s", diff->source, diff->target);

	return 0;
}

static int
open_diff_files(struct diff *diff)
{
	if ((diff->src = fopen(diff->source, "r")) == NULL) {
		err("cannot open source file '%s':", diff->source);
		return -1;
	}

	/* Make a temporary file in the same place */
	int n = snprintf(diff->tmpname, PATH_MAX, "%s.XXXXXX", diff->target);
	if (n >= PATH_MAX) {
		err("tmpname too long: %s.XXXXXX", diff->target);
		return -1;
	}
	int fd = mkstemp(diff->tmpname);
	if (fd == -1) {
		err("cannot create temporary file '%s':", diff->tmpname);
		return -1;
	}

	if ((diff->tmp = fdopen(fd, "w+")) == NULL) {
		err("fdopen failed:");
		unlink(diff->tmpname);
		close(fd);
		return -1;
	}

	return 0;
}

static int
close_diff_files(struct diff *diff)
{
	if (fclose(diff->tmp) != 0) {
		err("fclose tmp failed:");
		return -1;
	}

	if (fclose(diff->src) != 0) {
		err("fclose src failed:");
		return -1;
	}

	return 0;
}

static int
move_tmp_to_target(struct diff *diff)
{
	if (rename(diff->tmpname, diff->target) != 0) {
		err("rename failed:");
		return -1;
	}

	return 0;
}

static int
parse_diff(char *buf)
{
	struct diff *diff = calloc(1, sizeof(struct diff));
	if (diff == NULL) {
		err("calloc failed:");
		return -1;
	}

	if (parse_diff_filenames(diff, buf) != 0) {
		err("cannot parse diff filenames");
		return -1;
	}

	if (open_diff_files(diff) != 0) {
		err("cannot open diff files");
		return -1;
	}

	if (process_diff(diff) != 0) {
		err("cannot apply diff");
		return -1;
	}

	if (complete_diff(diff) != 0) {
		err("cannot complete diff");
		return -1;
	}

	if (close_diff_files(diff) != 0) {
		err("cannot close diff files");
		return -1;
	}

	if (move_tmp_to_target(diff) != 0) {
		err("cannot move tmp file to target");
		return -1;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	/* TODO: Handle long lines */
	char buf[4096];
	while (next_line(buf, 4096) == 0) {
		if (strncmp(buf, "--- ", 4) == 0) {
			if (parse_diff(buf) != 0) {
				err("diff failed");
				return 1;
			}
		}
	}

	return 0;
}
