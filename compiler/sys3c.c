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

static const char short_options[] = "a:E:ghi:Io:p:s:uV:v";
static const struct option long_options[] = {
	{ "ald",       required_argument, NULL, 'o' },
	{ "debug",     no_argument,       NULL, 'g' },
	{ "encoding",  required_argument, NULL, 'E' },
	{ "hed",       required_argument, NULL, 'i' },
	{ "help",      no_argument,       NULL, 'h' },
	{ "init",      no_argument,       NULL, 'I' },
	{ "project",   required_argument, NULL, 'p' },
	{ "sys-ver",   required_argument, NULL, 's' },
	{ "unicode",   no_argument,       NULL, 'u' },
	{ "variables", required_argument, NULL, 'V' },
	{ "version",   no_argument,       NULL, 'v' },
	{ 0, 0, 0, 0 }
};

static void usage(void) {
	puts("Usage: sys3c [options] file...");
	puts("Options:");
	puts("    -o, --ald <name>          Write output to <name> (default: " DEFAULT_ADISK_NAME ")");
	puts("    -g, --debug               Generate debug information");
	puts("    -Es, --encoding=sjis      Set input coding system to SJIS");
	puts("    -Eu, --encoding=utf8      Set input coding system to UTF-8 (default)");
	puts("    -i, --hed <file>          Read compile header (.hed) from <file>");
	puts("    -h, --help                Display this message and exit");
	puts("    -I, --init                Create a new sys3c project");
	puts("    -p, --project <file>      Read project configuration from <file>");
	puts("    -s, --sys-ver <ver>       Target System version (1|2|3(default))");
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
	char *buf = malloc(size + 1);
	if (size > 0 && fread(buf, size, 1, fp) != 1)
		error("%s: read error", path);
	fclose(fp);
	buf[size] = '\0';

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

static Vector *read_var_list(const char *path) {
	char *buf = read_file(path);
	Vector *vars = new_vec();
	char *line;
	while ((line = next_line(&buf)) != NULL)
		vec_push(vars, trim_right(line));
	return vars;
}

static void read_hed(const char *path, Vector *sources) {
	char *buf = read_file(path);
	char *dir = dirname_utf8(path);
	enum { INITIAL, SYSTEM35 } section = INITIAL;

	char *line;
	while ((line = next_line(&buf)) != NULL) {
		if (line[0] == '\x1a')  // DOS EOF
			break;
		if (line[0] == '#') {  // Section header
			trim_right(line);
			if (!strcmp("#SYSTEM35", line))
				section = SYSTEM35;
			else
				error("%s: unknown section %s", path, line);
			continue;
		}
		char *sc = strchr(line, ';');
		if (sc)
			*sc = '\0';
		trim_right(line);
		if (!line[0])
			continue;
		switch (section) {
		case INITIAL:
			error("%s: syntax error", path);
			break;
		case SYSTEM35:
			vec_push(sources, path_join(dir, line));
			break;
		}
	}
}

static void build(Vector *src_paths, Vector *variables, const char *adisk_name) {
	Map *srcs = new_map();
	for (int i = 0; i < src_paths->len; i++) {
		char *path = src_paths->data[i];
		map_put(srcs, path, read_file(path));
	}

	Compiler *compiler = new_compiler(srcs->keys, variables);

	if (config.debug)
		compiler->dbg_info = new_debug_info(srcs);

	for (int i = 0; i < srcs->keys->len; i++) {
		const char *source = srcs->vals->data[i];
		preprocess(compiler, source, i);
	}

	preprocess_done(compiler);

	uint32_t ald_mask = 0;
	Vector *ald = new_vec();
	for (int i = 0; i < srcs->keys->len; i++) {
		const char *source = srcs->vals->data[i];
		Sco *sco = compile(compiler, source, i);
		AldEntry *e = calloc(1, sizeof(AldEntry));
		e->volume = sco->ald_volume;
		e->data = sco->buf->buf;
		e->size = sco->buf->len;
		vec_push(ald, e);
		if (0 < e->volume && e->volume <= 26)
			ald_mask |= 1 << e->volume;
	}

	for (int i = 1; i <= 26; i++) {
		if (!(ald_mask & 1 << i))
			continue;
		char ald_path[PATH_MAX+1];
		strncpy(ald_path, adisk_name, PATH_MAX);
		if (i != 1) {
			char *base = strrchr(ald_path, '/');
			base = base ? base + 1 : ald_path;
			if (toupper(*base) != 'A')
				error("cannot determine output filename");
			*base += i - 1;
		}
		FILE *fp = checked_fopen(ald_path, "wb");
		ald_write(ald, i, fp);
		fclose(fp);
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
	bool init_mode = false;

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
		case 'h':
			usage();
			return 0;
		case 'i':
			hed = optarg;
			break;
		case 'I':
			init_mode = true;
			break;
		case 'o':
			adisk_name = optarg;
			break;
		case 'p':
			project = optarg;
			break;
		case 's':
			set_sys_ver(optarg);
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

	if (init_mode)
		return init_project(project, hed, adisk_name);

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

	Vector *vars = var_list ? read_var_list(var_list) : NULL;

	build(srcs, vars, adisk_name);
	return 0;
}
