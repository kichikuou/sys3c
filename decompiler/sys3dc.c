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
#include "sys3dc.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char short_options[] = "adE:ho:s:Vv";
static const struct option long_options[] = {
	{ "address",  no_argument,       NULL, 'a' },
	{ "encoding", required_argument, NULL, 'E' },
	{ "help",     no_argument,       NULL, 'h' },
	{ "outdir",   required_argument, NULL, 'o' },
	{ "sys-ver",  required_argument, NULL, 's' },
	{ "verbose",  no_argument,       NULL, 'V' },
	{ "version",  no_argument,       NULL, 'v' },
	{ 0, 0, 0, 0 }
};

static void usage(void) {
	puts("Usage: sys3dc [options] aldfile(s)");
	puts("Options:");
	puts("    -a, --address             Prefix each line with address");
	puts("    -Es, --encoding=sjis      Output files in SJIS encoding");
	puts("    -Eu, --encoding=utf8      Output files in UTF-8 encoding (default)");
	puts("    -h, --help                Display this message and exit");
	puts("    -o, --outdir <directory>  Write output into <directory>");
	puts("    -s, --sys-ver <ver>       NACT system version (1|2|3(default))");
	puts("    -V, --verbose             Be verbose");
	puts("    -v, --version             Print version information and exit");
}

static void version(void) {
	puts("sys3dc " VERSION);
}

static Sco *sco_new(int page, const uint8_t *data, int len, int volume) {
	char name[10];
	Sco *sco = calloc(1, sizeof(Sco));
	sco->data = data;
	sco->mark = calloc(1, len + 1);
	sprintf(name, "%d.sco", page);
	sco->sco_name = strdup(name);
	sprintf(name, "%d.adv", page);
	sco->src_name = strdup(name);
	sco->ald_volume = volume;
	sco->hdrsize = 2;
	sco->filesize = len;  // `*(uint16_t*)data` is filesize, but don't trust it.
	sco->page = page;

	if (len < sco->filesize) {
		error("%s: unexpected file size in SCO header (expected %d, got %d)",
			  sjis2utf(name), len, sco->filesize);
	}
	return sco;
}

void convert_to_utf8(FILE *fp) {
	fseek(fp, 0, SEEK_SET);
	Vector *lines = new_vec();
	char buf[2048];
	while (fgets(buf, sizeof(buf), fp))
		vec_push(lines, sjis2utf(buf));

	fseek(fp, 0, SEEK_SET);
	for (int i = 0; i < lines->len; i++)
		fputs(lines->data[i], fp);
	// No truncation needed because UTF-8 encoding is no shorter than SJIS.
}

const char *to_utf8(const char *s) {
	if (config.utf8_input)
		return s;
	return sjis2utf(s);
}

int main(int argc, char *argv[]) {
	init(&argc, &argv);

	const char *outdir = NULL;

	int opt;
	while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch (opt) {
		case 'a':
			config.address = true;
			break;
		case 'E':
			switch (optarg[0]) {
			case 's': case 'S': config.utf8_output = false; break;
			case 'u': case 'U': config.utf8_output = true; break;
			default: error("Unknown encoding %s", optarg);
			}
			break;
		case 'h':
			usage();
			return 0;
		case 'o':
			outdir = optarg;
			break;
		case 's':
			switch (optarg[0]) {
			case '1': config.sys_ver = SYSTEM1; break;
			case '2': config.sys_ver = SYSTEM2; break;
			case '3': config.sys_ver = SYSTEM3; break;
			default: error("Invalid system version '%s'", optarg);
			}
			break;
		case 'V':
			config.verbose = true;
			break;
		case 'v':
			version();
			return 0;
		case '?':
			usage();
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		return 1;
	}

	Vector *scos = NULL;
	const char *adisk_name = NULL;
	for (int i = 0; i < argc; i++) {
		scos = ald_read(scos, argv[i]);
		char *basename = basename_utf8(argv[i]);
		if (toupper(basename[0]) == 'A')
			adisk_name = basename;
	}

	for (int i = 0; i < scos->len; i++) {
		AldEntry *e = scos->data[i];
		if (!e)
			continue;
		Sco *sco = sco_new(i + 1, e->data, e->size, e->volume);
		scos->data[i] = sco;

		// Detect unicode SCO.
		if (i == 0 && !memcmp(sco->data + sco->hdrsize, "ZU\x41\x7f", 4))
			config.utf8_input = true;
	}
	if (config.utf8_input && !config.utf8_output)
		error("Unicode game data cannot be decompiled with -Es.");

	if (outdir && make_dir(outdir) != 0 && errno != EEXIST)
		error("cannot create directory %s: %s", outdir, strerror(errno));

	decompile(scos, outdir, adisk_name);

	return 0;
}
