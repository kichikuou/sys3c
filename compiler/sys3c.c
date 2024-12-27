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
#include "sys3c.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_ADISK_NAME "ADISK.DAT"

static const char short_options[] = "a:E:G:ghi:o:p:uV:v";
static const struct option long_options[] = {
	{ "dri",       required_argument, NULL, 'o' },
	{ "debug",     no_argument,       NULL, 'g' },
	{ "encoding",  required_argument, NULL, 'E' },
	{ "game",      required_argument, NULL, 'G' },
	{ "hed",       required_argument, NULL, 'i' },
	{ "help",      no_argument,       NULL, 'h' },
	{ "project",   required_argument, NULL, 'p' },
	{ "unicode",   no_argument,       NULL, 'u' },
	{ "variables", required_argument, NULL, 'V' },
	{ "version",   no_argument,       NULL, 'v' },
	{ 0, 0, 0, 0 }
};

static void usage(void) {
	puts("Usage: sys3c [options] file...");
	puts("Options:");
	puts("    -o, --dri <name>          Write output to <name> (default: " DEFAULT_ADISK_NAME ")");
	puts("    -g, --debug               Generate debug information");
	puts("    -G, --game <id>           Specify game ID");
	puts("    -Es, --encoding=sjis      Set input coding system to SJIS");
	puts("    -Eu, --encoding=utf8      Set input coding system to UTF-8 (default)");
	puts("    -i, --hed <file>          Read compile header (.hed) from <file>");
	puts("    -h, --help                Display this message and exit");
	puts("    -p, --project <file>      Read project configuration from <file>");
	puts("    -u, --unicode             Generate Unicode output (can only be run on xsystem35)");
	puts("    -V, --variables <file>    Read list of variables from <file>");
	puts("    -v, --version             Print version information and exit");
}

static void version(void) {
	puts("sys3c " VERSION);
}

static char *next_line(char **buf) {
	if (!**buf)
		return NULL;
	char *line = *buf;
	char *p = strchr(line, '\n');
	if (p) {
		*p = '\0';
		*buf = p + 1;
	} else {
		*buf += strlen(line);
	}
	return line;
}

static char *read_file(const char *path) {
	FILE *fp = checked_fopen(path, "rb");
	if (fseek(fp, 0, SEEK_END) != 0)
		error("%s: %s", path, strerror(errno));
	long size = ftell(fp);
	if (size < 0)
		error("%s: %s", path, strerror(errno));
	if (fseek(fp, 0, SEEK_SET) != 0)
		error("%s: %s", path, strerror(errno));
	char *buf = malloc(size + 2);
	if (size > 0 && fread(buf, size, 1, fp) != 1)
		error("%s: read error", path);
	fclose(fp);
	buf[size] = '\n';
	buf[size + 1] = '\0';

	if (config.utf8) {
		const char *err = validate_utf8(buf);
		if (err) {
			lexer_init(buf, path, -1);
			error_at(err, "Invalid UTF-8 character");
		}
		return buf;
	} else {
		char *utf = sjis2utf_sub(buf, 0xfffd);  // U+FFFD REPLACEMENT CHARACTER
		char *err = strstr(utf, u8"\ufffd");
		if (err) {
			lexer_init(utf, path, -1);
			error_at(err, "Invalid Shift_JIS character");
		}
		return sjis2utf(buf);
	}
}

static char *trim_right(char *str) {
	for (char *p = str + strlen(str) - 1; p >= str && isspace(*p); p--)
		*p = '\0';
	return str;
}

static Vector *read_txt(const char *path) {
	char *buf = read_file(path);
	Vector *vars = new_vec();
	char *line;
	while ((line = next_line(&buf)) != NULL)
		vec_push(vars, trim_right(line));
	// Drop empty lines at the end
	while (vars->len > 0 && !((char *)vars->data[vars->len - 1])[0])
		vars->len--;
	return vars;
}

static void read_hed(const char *path, Vector *sources) {
	char *buf = read_file(path);
	char *dir = dirname_utf8(path);

	char *line;
	while ((line = next_line(&buf)) != NULL) {
		trim_right(line);
		vec_push(sources, *line ? path_join(dir, line) : NULL);
	}
	// Drop empty lines at the end
	while (sources->len > 0 && !sources->data[sources->len - 1])
		sources->len--;
}

static void build(Vector *src_paths, Vector *variables, Vector *verbs, Vector *objs, const char *adisk_name) {
	Map *srcs = new_map();
	for (int i = 0; i < src_paths->len; i++) {
		char *path = src_paths->data[i];
		map_put(srcs, path, path ? read_file(path) : NULL);
	}

	Compiler *compiler = new_compiler(srcs->keys, variables, verbs, objs);

	if (config.debug)
		compiler->dbg_info = new_debug_info(srcs);

	uint32_t dri_mask = 0;
	Vector *dri = new_vec();
	for (int i = 0; i < srcs->keys->len; i++) {
		const char *source = srcs->vals->data[i];
		if (!source) {
			vec_push(dri, NULL);
			if (config.debug) {
				debug_init_page(compiler->dbg_info, i);
				debug_finish_page(compiler->dbg_info);
			}
			continue;
		}
		Sco *sco = compile(compiler, source, i);
		DriEntry *e = calloc(1, sizeof(DriEntry));
		e->volume_bits = sco->volume_bits;
		e->data = sco->buf->buf;
		e->size = sco->buf->len;
		vec_push(dri, e);
		dri_mask |= e->volume_bits;
	}

	for (int i = 1; i <= DRI_MAX_VOLUME; i++) {
		if (!(dri_mask & 1 << i))
			continue;
		char dri_path[PATH_MAX+1];
		strncpy(dri_path, adisk_name, PATH_MAX);
		if (i != 1) {
			char *base = strrchr(dri_path, '/');
			base = base ? base + 1 : dri_path;
			if (toupper(*base) != 'A')
				error("cannot determine output filename");
			*base += i - 1;
		}
		FILE *fp = checked_fopen(dri_path, "wb");
		dri_write(dri, i, fp);
		fclose(fp);
	}

	if (verbs) {
		if (!config.unicode) {
			for (int i = 0; i < verbs->len; i++)
				verbs->data[i] = utf2sjis(verbs->data[i]);
			for (int i = 0; i < objs->len; i++)
				objs->data[i] = utf2sjis(objs->data[i]);
		}
		AG00 ag00 = {
			.verbs = verbs,
			.objs = objs,
			.uk1 = config.ag00_uk1,
			.uk2 = config.ag00_uk2,
		};
		ag00_write(&ag00, path_join(dirname_utf8(adisk_name), "AG00.DAT"));
	}

	if (config.debug) {
		char symbols_path[PATH_MAX+1];
		snprintf(symbols_path, sizeof(symbols_path), "%s.symbols", adisk_name);
		FILE *fp = checked_fopen(symbols_path, "wb");
		debug_info_write(compiler->dbg_info, compiler, fp);
		fclose(fp);
	}
}

int main(int argc, char *argv[]) {
	init(&argc, &argv);

	const char *project = NULL;
	const char *adisk_name = NULL;
	const char *hed = NULL;
	const char *var_list = NULL;

	int opt;
	while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch (opt) {
		case 'E':
			switch (optarg[0]) {
			case 's': case 'S': config.utf8 = false; break;
			case 'u': case 'U': config.utf8 = true; break;
			default: error("Unknown encoding %s", optarg);
			}
			break;
		case 'g':
			config.debug = true;
			break;
		case 'G':
			config.game_id = game_id_from_name(optarg);
			if (config.game_id == UNKNOWN_GAME)
				error("Unknown game ID '%s'", optarg);
			config.sys_ver = get_sysver(config.game_id);
			break;
		case 'h':
			usage();
			return 0;
		case 'i':
			hed = optarg;
			break;
		case 'o':
			adisk_name = optarg;
			break;
		case 'p':
			project = optarg;
			break;
		case 'u':
			config.unicode = true;
			break;
		case 'V':
			var_list = optarg;
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

	if (project) {
		FILE *fp = checked_fopen(project, "r");
		load_config(fp, dirname_utf8(project));
		fclose(fp);
	} else if (!hed && argc == 0) {
		FILE *fp = fopen("sys3c.cfg", "r");
		if (fp) {
			load_config(fp, NULL);
			fclose(fp);
		} else {
			usage();
			return 1;
		}
	}
	if (!hed && config.hed)
		hed = config.hed;
	if (!var_list && config.var_list)
		var_list = config.var_list;
	if (!adisk_name) {
		adisk_name = config.adisk_name ? config.adisk_name
			: project ? path_join(dirname_utf8(project), DEFAULT_ADISK_NAME)
			: DEFAULT_ADISK_NAME;
	}

	Vector *srcs = new_vec();
	if (hed)
		read_hed(hed, srcs);

	for (int i = 0; i < argc; i++)
		vec_push(srcs, argv[i]);

	if (srcs->len == 0)
		error("sys3c: No source file specified.");

	Vector *vars = var_list ? read_txt(var_list) : NULL;
	Vector *verbs = config.verb_list ? read_txt(config.verb_list) : NULL;
	if (verbs && verbs->len > 256)
		error("Too many verbs");
	Vector *objs = config.obj_list ? read_txt(config.obj_list) : NULL;
	if (objs && objs->len > 256)
		error("Too many objects");

	build(srcs, vars, verbs, objs, adisk_name);
	return 0;
}
