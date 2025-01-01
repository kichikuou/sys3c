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
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char short_options[] = "adE:g:ho:uVv";
static const struct option long_options[] = {
	{ "address",  no_argument,       NULL, 'a' },
	{ "encoding", required_argument, NULL, 'E' },
	{ "game",     required_argument, NULL, 'G' },
	{ "help",     no_argument,       NULL, 'h' },
	{ "outdir",   required_argument, NULL, 'o' },
	{ "unicode",  no_argument,       NULL, 'u' },
	{ "verbose",  no_argument,       NULL, 'V' },
	{ "version",  no_argument,       NULL, 'v' },
	{ 0, 0, 0, 0 }
};

static void usage(void) {
	puts("Usage: sys3dc [options] gamedir|datfile(s)");
	puts("Options:");
	puts("    -a, --address             Prefix each line with address");
	puts("    -Es, --encoding=sjis      Output files in SJIS encoding");
	puts("    -Eu, --encoding=utf8      Output files in UTF-8 encoding (default)");
	puts("    -G, --game <id>           Specify game ID");
	puts("    -h, --help                Display this message and exit");
	puts("    -o, --outdir <directory>  Write output into <directory>");
	puts("    -u, --unicode             Decompile Unicode game data");
	puts("    -V, --verbose             Be verbose");
	puts("    -v, --version             Print version information and exit");
}

static void version(void) {
	puts("sys3dc " VERSION);
}

static Sco *sco_new(int page, const uint8_t *data, int len, uint32_t volume_bits) {
	char name[10];
	Sco *sco = calloc(1, sizeof(Sco));
	sco->data = data;
	sco->mark = calloc(1, len + 1);
	sprintf(name, "%d.sco", page);
	sco->sco_name = strdup(name);
	sprintf(name, "%d.adv", page);
	sco->src_name = strdup(name);
	sco->volume_bits = volume_bits;
	sco->default_addr = le16(data);
	sco->page = page;

	// Trim trailing 0x00.
	while (len > 0 && data[len - 1] == 0)
		len--;
	sco->filesize = len;

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

static bool is_directory(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void find_input_files(const char *dir, int *argc, char ***argv) {
	Vector *files = new_vec();
	DIR *dp = opendir(dir);
	if (!dp)
		error("%s: %s", dir, strerror(errno));
	struct dirent *d;
	while ((d = readdir(dp))) {
		if ((isalpha(d->d_name[0]) && !strcasecmp(d->d_name + 1, "DISK.DAT")) ||
				!strcasecmp(d->d_name, "AG00.DAT")) {
			vec_push(files, path_join(dir, d->d_name));
		}
	}
	closedir(dp);
	*argc = files->len;
	*argv = calloc(files->len, sizeof(char *));
	for (int i = 0; i < files->len; i++)
		(*argv)[i] = files->data[i];
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
		case 'G':
			config.game_id = game_id_from_name(optarg);
			if (config.game_id == UNKNOWN_GAME)
				error("Unknown game ID %s", optarg);
			config.sys_ver = get_sysver(config.game_id);
			break;
		case 'h':
			usage();
			return 0;
		case 'o':
			outdir = optarg;
			break;
		case 'u':
			config.utf8_input = true;
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

	if (argc == 1 && is_directory(argv[0])) {
		const char *dir = argv[0];
		find_input_files(dir, &argc, &argv);
		if (argc == 0) {
			if (!strcmp(dir, "."))
				dir = "current directory";
			error("ADISK.DAT is not found in %s", dir);
		}
	}

	Vector *scos = NULL;
	AG00 *ag00 = NULL;
	const char *adisk_name = NULL;
	uint32_t adisk_crc = 0, bdisk_crc = 0;
	for (int i = 0; i < argc; i++) {
		char *basename = basename_utf8(argv[i]);
		if (!strcasecmp(basename, "AG00.DAT")) {
			ag00 = ag00_read(argv[i]);
			continue;
		}
		scos = dri_read(scos, argv[i]);
		switch (toupper(basename[0])) {
		case 'A':
			adisk_name = basename;
			adisk_crc = calc_crc32(argv[i]);
			break;
		case 'B':
			bdisk_crc = calc_crc32(argv[i]);
			break;
		}
	}
	if (config.game_id == UNKNOWN_GAME) {
		config.game_id = detect_game_id(adisk_crc, bdisk_crc);
		if (config.game_id == UNKNOWN_GAME)
			error("Cannot detect game ID. Please specify --game.");
		config.sys_ver = get_sysver(config.game_id);
	}

	for (int i = 0; i < scos->len; i++) {
		DriEntry *e = scos->data[i];
		if (!e)
			continue;
		Sco *sco = sco_new(i + 1, e->data, e->size, e->volume_bits);
		scos->data[i] = sco;
	}
	if (config.utf8_input && !config.utf8_output)
		error("Unicode game data cannot be decompiled with -Es.");

	if (outdir && make_dir(outdir) != 0 && errno != EEXIST)
		error("cannot create directory %s: %s", outdir, strerror(errno));

	decompile(scos, ag00, outdir, adisk_name);

	return 0;
}
