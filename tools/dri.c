/* Copyright (C) 2020 <KichikuouChrome@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/
#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <sys/utime.h>
#endif

static void usage(void) {
	puts("Usage: dri <command> [<args>]");
	puts("");
	puts("commands:");
	puts("  list     Print list of archive files");
	puts("  create   Create a new archive");
	puts("  extract  Extract file(s) from archive");
	puts("  dump     Print hex dump of file");
	puts("  compare  Compare contents of two archives");
	puts("  help     Display help information about commands");
	puts("  version  Display version information and exit");
	puts("");
	puts("Run 'dri help <command>' for more information about a specific command.");
}

static bool is_archive_filename(const char *path) {
	// *.DAT
	const char *dot = strrchr(path, '.');
	if (dot && !strcasecmp(dot, ".DAT"))
		return true;
	// *-?
	const char *hyphen = strrchr(path, '-');
	if (hyphen && isalpha(hyphen[1]) && hyphen[2] == '\0')
		return true;
	return false;
}

static Vector *read_dris(int *pargc, char **pargv[]) {
	int argc = *pargc;
	char **argv = *pargv;

	for (int dd = 0; dd < argc; dd++) {
		if (!strcmp(argv[dd], "--")) {
			Vector *dri = NULL;
			for (int i = 0; i < dd; i++)
				dri = dri_read(dri, argv[i]);
			*pargc -= dd + 1;
			*pargv += dd + 1;
			return dri;
		}
	}

	Vector *dri = NULL;
	for (int i = 0; i < argc; i++) {
		if (!is_archive_filename(argv[i]))
			break;
		dri = dri_read(dri, argv[i]);
		*pargc -= 1;
		*pargv += 1;
	}
	return dri;
}

static DriEntry *find_entry(Vector *dri, const char *num_or_name) {
	char *endptr;
	unsigned long idx = strtoul(num_or_name, &endptr, 0);
	if (*endptr == '\0') {
		if (idx == 0 || idx > dri->len) {
			fprintf(stderr, "dri: index %lu is out of range (1-%d)\n", idx, dri->len);
			return NULL;
		}
		if (!dri->data[idx-1]) {
			fprintf(stderr, "dri: No entry for index %lu\n", idx);
			return NULL;
		}
		return dri->data[idx-1];
	}

	fprintf(stderr, "dri: No entry for '%s'\n", num_or_name);
	return NULL;
}

static void write_manifest(Vector *dri, FILE *fp) {
	for (int i = 0; i < dri->len; i++) {
		DriEntry *e = dri->data[i];
		if (!e) continue;
		for (int vol = 1; vol <= DRI_MAX_VOLUME; vol++) {
			if (e->volume_bits & 1 << vol)
				fputc(vol - 1 + 'A', fp);
		}
		fprintf(fp, ",%d,%d.out\n", i + 1, i + 1);
	}
}

// dri list ----------------------------------------

static void help_list(void) {
	puts("Usage: dri list <drifile>...");
}

static int do_list(int argc, char *argv[]) {
	if (argc == 1) {
		help_list();
		return 1;
	}
	Vector *dri = new_vec();
	for (int i = 1; i < argc; i++)
		dri_read(dri, argv[i]);
	for (int i = 0; i < dri->len; i++) {
		DriEntry *e = dri->data[i];
		if (!e)
			continue;
		printf("%4d\t", i + 1);
		for (int vol = 1; vol <= DRI_MAX_VOLUME; vol++) {
			if (e->volume_bits & 1 << vol)
				putchar(vol - 1 + 'A');
		}
		printf("\t%5d\n", e->size);
	}
	return 0;
}

// dri create ----------------------------------------

static const char create_short_options[] = "m:";
static const struct option create_long_options[] = {
	{ "manifest",  required_argument, NULL, 'm' },
	{ 0, 0, 0, 0 }
};

static void help_create(void) {
	puts("Usage: dri create <drifile> <file>...");
	puts("       dri create <drifile> -m <manifest-file>");
	puts("Options:");
	puts("    -m, --manifest <file>    Read manifest from <file>");
}

static void add_file(Vector *dri, uint32_t volume_bits, int no, const char *path) {
	FILE *fp = checked_fopen(path, "rb");
	struct stat sbuf;
	if (fstat(fileno(fp), &sbuf) < 0)
		error("%s: %s", path, strerror(errno));
	uint8_t *data = malloc(sbuf.st_size);
	if (!data)
		error("out of memory");
	if (sbuf.st_size > 0 && fread(data, sbuf.st_size, 1, fp) != 1)
		error("%s: %s", path, strerror(errno));
	fclose(fp);

	DriEntry *e = calloc(1, sizeof(DriEntry));
	e->data = data;
	e->size = sbuf.st_size;
	e->volume_bits = volume_bits;
	vec_set(dri, no - 1, e);
}

static uint32_t add_files_from_manifest(Vector *dri, const char *manifest) {
	FILE *fp = checked_fopen(manifest, "r");
	char line[200];
	int lineno = 0;
	uint32_t all_volumes = 0;
	while (fgets(line, sizeof(line), fp)) {
		lineno++;
		if (line[0] == '\n')
			continue;
		int link_no;
		char volume_letters[27];
		char fname[201];
		if (sscanf(line, " %26[^,], %d, %200s", volume_letters, &link_no, fname) != 3)
			error("%s:%d manifest syntax error", manifest, lineno);

		uint32_t volume_bits = 0;
		for (char *p = volume_letters; *p; p++) {
			if (!isalpha(*p))
				error("%s:%d invalid volume letter '%c'", manifest, lineno, *p);
			volume_bits |= 1 << (toupper(*p) - 'A' + 1);
		}
		if (link_no < 1 || link_no > 65535)
			error("%s:%d invalid link number %d", manifest, lineno, link_no);
		if (link_no - 1 < dri->len && dri->data[link_no - 1])
			error("%s:%d duplicated link number %d", manifest, lineno, link_no);
		all_volumes |= volume_bits;
		add_file(dri, volume_bits, link_no, fname);
	}
	fclose(fp);
	return all_volumes;
}

static int do_create(int argc, char *argv[]) {
	const char *manifest = NULL;
	int opt;
	while ((opt = getopt_long(argc, argv, create_short_options, create_long_options, NULL)) != -1) {
		switch (opt) {
		case 'm':
			manifest = optarg;
			break;
		default:
			help_create();
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	if ((manifest && argc != 1) || (!manifest && argc <= 1)) {
		help_create();
		return 1;
	}
	char *dri_path = strdup(argv[0]);
	Vector *entries = new_vec();

	if (manifest) {
		char *dirname = dirname_utf8(dri_path);
		char *basename = basename_utf8(dri_path);

		uint32_t vol_bits = add_files_from_manifest(entries, manifest);
		for (int vol = 1; vol <= DRI_MAX_VOLUME; vol++) {
			if ((vol_bits & 1 << vol) == 0)
				continue;
			char *dri_fname = strdup(basename);
			if (!dri_filename(dri_fname, vol))
				error("dri: cannot determine output filename");
			FILE *fp = checked_fopen(path_join(dirname, dri_fname), "wb");
			dri_write(entries, vol, fp);
			fclose(fp);
		}
	} else {
		for (int i = 1; i < argc; i++)
			add_file(entries, 1 << 1, i, argv[i]);
		FILE *fp = checked_fopen(dri_path, "wb");
		dri_write(entries, 1, fp);
		fclose(fp);
	}
	return 0;
}

// dri extract ----------------------------------------

static const char extract_short_options[] = "d:m:";
static const struct option extract_long_options[] = {
	{ "directory", required_argument, NULL, 'd' },
	{ "manifest",  required_argument, NULL, 'm' },
	{ 0, 0, 0, 0 }
};

static void help_extract(void) {
	puts("Usage: dri extract [options] <drifile>... [--] [(<n>|<file>)...]");
	puts("Options:");
	puts("    -d, --directory <dir>    Extract files into <dir>");
	puts("    -m, --manifest <file>    Write manifest to <file>");
}

static void extract_entry(DriEntry *e, const char *directory) {
	char name[20];
	sprintf(name, "%d.out", e->id);
	puts(name);
	FILE *fp = checked_fopen(path_join(directory, name), "wb");
	if (e->size > 0 && fwrite(e->data, e->size, 1, fp) != 1)
		error("%s: %s", name, strerror(errno));
	fclose(fp);
}

static int do_extract(int argc, char *argv[]) {
	const char *directory = NULL;
	const char *manifest = NULL;
	int opt;
	while ((opt = getopt_long(argc, argv, extract_short_options, extract_long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			directory = optarg;
			break;
		case 'm':
			manifest = optarg;
			break;
		default:
			help_extract();
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	Vector *dri = read_dris(&argc, &argv);
	if (!dri) {
		help_extract();
		return 1;
	}

	if (directory && make_dir(directory) != 0 && errno != EEXIST)
		error("cannot create directory %s: %s", directory, strerror(errno));

	if (manifest) {
		FILE *fp = checked_fopen(manifest, "w");
		write_manifest(dri, fp);
		fclose(fp);
	}

	if (!argc) {
		// Extract all files.
		for (int i = 0; i < dri->len; i++) {
			DriEntry *e = dri->data[i];
			if (e)
				extract_entry(e, directory);
		}
	} else {
		for (int i = 0; i < argc; i++) {
			DriEntry *e = find_entry(dri, argv[i]);
			if (e)
				extract_entry(e, directory);
		}
	}
	return 0;
}

// dri dump ----------------------------------------

static void help_dump(void) {
	puts("Usage: dri dump <drifile>... [--] <n>|<file>");
}

static void print_sjis_2byte(uint8_t c1, uint8_t c2) {
	char in[3] = {c1, c2, 0};
	char *out = sjis2utf_sub(in, '.');
	fputs(out, stdout);
}

static void dump_entry(DriEntry *entry) {
	bool skip_first = false;
	for (int addr = 0; addr < entry->size; addr += 16) {
		int n = (entry->size - addr > 16) ? 16 : entry->size - addr;
		printf("%08x: ", addr);
		for (int i = 0; i < n; i++)
			printf("%02x ", entry->data[addr + i]);
		for (int i = n; i < 16; i++)
			printf("   ");
		putchar(' ');

		const uint8_t *p = entry->data + addr;
		for (int i = 0; i < n; i++) {
			if (i == 0 && skip_first) {
				putchar(' ');
				skip_first = false;
				continue;
			}
			if (is_sjis_byte1(p[i])) {
				if (addr + i + 1 < entry->size && is_sjis_byte2(p[i+1])) {
					print_sjis_2byte(p[i], p[i+1]);
					if (i == 15)
						skip_first = true;
					i++;
				} else {
					putchar('.');
				}
			} else if (is_sjis_half_kana(p[i])) {
				print_sjis_2byte(p[i], 0);
			} else if (isprint(p[i])) {
				putchar(p[i]);
			} else {
				putchar('.');
			}
		}
		putchar('\n');
	}
}

static int do_dump(int argc, char *argv[]) {
	argc--;
	argv++;
	Vector *dri = read_dris(&argc, &argv);
	if (!dri || argc != 1) {
		help_dump();
		return 1;
	}

	DriEntry *e = find_entry(dri, argv[0]);
	if (!e)
		return 1;
	dump_entry(e);
	return 0;
}

// dri compare ----------------------------------------

static void help_compare(void) {
	puts("Usage: dri compare <drifile1> <drifile2>");
}

static bool compare_entry(int page, DriEntry *e1, DriEntry *e2) {
	if (e1->size == e2->size && !memcmp(e1->data, e2->data, e1->size))
		return false;

	int i;
	for (i = 0; i < e1->size && i < e2->size; i++) {
		if (e1->data[i] != e2->data[i])
			break;
	}
	printf("page %d: differ at %05x\n", page, i);
	return true;
}

static bool compare_ag00(const char *file1, const char *file2) {
	AG00 *ag00_1 = ag00_read(file1);
	AG00 *ag00_2 = ag00_read(file2);

	if (ag00_1->verbs->len != ag00_2->verbs->len) {
		printf("number of verbs differ (%d vs %d)\n", ag00_1->verbs->len, ag00_2->verbs->len);
		return 1;
	} else {
		for (int i = 0; i < ag00_1->verbs->len; i++) {
			if (strcmp(ag00_1->verbs->data[i], ag00_2->verbs->data[i])) {
				printf("verb %d differ\n", i);
				return 1;
			}
		}
	}
	if (ag00_1->objs->len != ag00_2->objs->len) {
		printf("number of objects differ (%d vs %d)\n", ag00_1->objs->len, ag00_2->objs->len);
		return 1;
	} else {
		for (int i = 0; i < ag00_1->objs->len; i++) {
			if (strcmp(ag00_1->objs->data[i], ag00_2->objs->data[i])) {
				printf("object %d differ\n", i);
				return 1;
			}
		}
	}
	if (ag00_1->uk1 != ag00_2->uk1) {
		printf("uk1 differ (%d vs %d)\n", ag00_1->uk1, ag00_2->uk1);
		return 1;
	}
	if (ag00_1->uk2 != ag00_2->uk2) {
		printf("uk2 differ (%d vs %d)\n", ag00_1->uk2, ag00_2->uk2);
		return 1;
	}
	return 0;
}

static int do_compare(int argc, char *argv[]) {
	if (argc != 3) {
		help_compare();
		return 1;
	}
	const char *drifile1 = argv[1];
	const char *drifile2 = argv[2];
	if ((!strcasecmp(basename_utf8(drifile1) + 1, "G00.DAT") &&
	     !strcasecmp(basename_utf8(drifile2) + 1, "G00.DAT")) ||
	    (!strcasecmp(basename_utf8(drifile1), "AO00.ASC") &&
	     !strcasecmp(basename_utf8(drifile2), "AO00.ASC"))) {
		return compare_ag00(drifile1, drifile2);
	}

	Vector *dri1 = dri_read(NULL, drifile1);
	Vector *dri2 = dri_read(NULL, drifile2);

	bool differs = false;
	for (int i = 0; i < dri1->len && i < dri2->len; i++) {
		if (dri1->data[i] && dri2->data[i]) {
			differs |= compare_entry(i + 1, dri1->data[i], dri2->data[i]);
		} else if (dri1->data[i]) {
			printf("page %d only exists in %s\n", i + 1, drifile1);
			differs = true;
		} else if (dri2->data[i]) {
			printf("page %d only exists in %s\n", i + 1, drifile2);
			differs = true;
		}
	}

	for (int i = dri2->len; i < dri1->len; i++) {
		printf("page %d only exists in %s\n", i + 1, drifile1);
		differs = true;
	}
	for (int i = dri1->len; i < dri2->len; i++) {
		printf("page %d only exists in %s\n", i + 1, drifile2);
		differs = true;
	}

	if (!differs) {
		// Make sure the CRC32 of the first 256 bytes are the same.
		uint32_t crc1 = calc_crc32(drifile1);
		uint32_t crc2 = calc_crc32(drifile2);
		if (crc1 != crc2) {
			printf("CRC32 of the first 256 bytes differ: %08x vs %08x\n", crc1, crc2);
			differs = true;
		}
	}

	return differs ? 1 : 0;
}

// dri help ----------------------------------------

static void help_help(void) {
	puts("Usage: dri help <command>");
}

static int do_help(int argc, char *argv[]);  // defined below

// dri version ----------------------------------------

static void help_version(void) {
	puts("Usage: dri version");
}

static int do_version(int argc, char *argv[]) {
	puts("dri " VERSION);
	return 0;
}

// main ----------------------------------------

typedef struct {
	const char *name;
	int (*func)(int argc, char *argv[]);
	void (*help)(void);
} Command;

static Command commands[] = {
	{"list",    do_list,    help_list},
	{"create",  do_create,  help_create},
	{"extract", do_extract, help_extract},
	{"dump",    do_dump,    help_dump},
	{"compare", do_compare, help_compare},
	{"help",    do_help,    help_help},
	{"version", do_version, help_version},
	{NULL, NULL, NULL}
};

// Defined here because it accesses the commands table.
static int do_help(int argc, char *argv[]) {
	if (argc == 1) {
		help_help();
		return 1;
	}
	for (Command *cmd = commands; cmd->name; cmd++) {
		if (!strcmp(argv[1], cmd->name)) {
			cmd->help();
			return 0;
		}
	}
	error("dri help: Invalid subcommand '%s'", argv[1]);
}

int main(int argc, char *argv[]) {
	init(&argc, &argv);

	if (argc == 1) {
		usage();
		return 1;
	}
	for (Command *cmd = commands; cmd->name; cmd++) {
		if (!strcmp(argv[1], cmd->name))
			return cmd->func(argc - 1, argv + 1);
	}
	error("dri: Invalid subcommand '%s'", argv[1]);
}
