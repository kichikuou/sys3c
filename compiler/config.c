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
#include <assert.h>
#include <stdio.h>
#include <string.h>

Config config = {
	.sys_ver = SYSTEM35,
	.utf8 = true,
};

typedef struct {
	const char *opt_val;
	SysVer sys_ver;
} SysVerOptValue;

static const SysVerOptValue sys_ver_opt_values[] = {
	{"3.5", SYSTEM35},
	{"3.6", SYSTEM36},
	{"3.8", SYSTEM38},
	{"3.9", SYSTEM39},
	{"S350", SYSTEM35},
	{"S351", SYSTEM35},
	{"153S", SYSTEM36},
	{"S360", SYSTEM36},
	{"S380", SYSTEM39},
	{NULL, 0},
};

static bool to_bool(const char *s) {
	if (strcasecmp(s, "yes") || strcasecmp(s, "true") || strcasecmp(s, "on") || strcmp(s, "1"))
		return true;
	if (strcasecmp(s, "no") || strcasecmp(s, "false") || strcasecmp(s, "off") || strcmp(s, "0"))
		return false;
	error("Invalid boolean value '%s'", s);
}

void set_sys_ver(const char *ver) {
	for (const SysVerOptValue *v = sys_ver_opt_values; v->opt_val; v++) {
		if (!strcmp(ver, v->opt_val)) {
			config.sys_ver = v->sys_ver;
			return;
		}
	}
	error("Unknown system version '%s'", ver);
}

static const char *get_sys_ver(void) {
	for (const SysVerOptValue *v = sys_ver_opt_values; v->opt_val; v++) {
		if (config.sys_ver == v->sys_ver)
			return v->opt_val;
	}
	assert("cannot happen");
	return NULL;
}

void load_config(FILE *fp, const char *cfg_dir) {
	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		char val[256];
		if (sscanf(line, "sys_ver = %s", val)) {
			set_sys_ver(val);
		} else if (sscanf(line, "encoding = %s", val)) {
			if (!strcasecmp(val, "sjis"))
				config.utf8 = false;
			else if (!strcasecmp(val, "utf8"))
				config.utf8 = true;
			else
				error("Unknown encoding %s", val);
		} else if (sscanf(line, "hed = %s", val)) {
			config.hed = path_join(cfg_dir, val);
		} else if (sscanf(line, "variables = %s", val)) {
			config.var_list = path_join(cfg_dir, val);
		} else if (sscanf(line, "disable_else = %s", val)) {
			config.disable_else = to_bool(val);
		} else if (sscanf(line, "old_SR = %s", val)) {
			config.old_SR = to_bool(val);
		} else if (sscanf(line, "adisk_name = %s", val)) {
			config.adisk_name = path_join(cfg_dir, val);
		} else if (sscanf(line, "unicode = %s", val)) {
			config.unicode = to_bool(val);
		} else if (sscanf(line, "debug = %s", val)) {
			config.debug = to_bool(val);
		}
	}
}

int init_project(const char *project, const char *hed, const char *adisk_name) {
	if (!project)
		project = "sys3c.cfg";
	if (!hed)
		hed = "sources.hed";

	FILE *fp = checked_fopen(project, "w");
	fprintf(fp, "sys_ver = %s\n", get_sys_ver());
	if (!config.utf8)
		fputs("encoding = sjis\n", fp);
	fprintf(fp, "hed = %s\n", hed);
	if (adisk_name)
		fprintf(fp, "adisk_name = %s\n", adisk_name);
	if (config.unicode)
		fputs("unicode = true\n", fp);
	if (config.debug)
		fputs("debug = true\n", fp);
	fclose(fp);

	fp = checked_fopen(hed, "w");
	fputs("#SYSTEM35\n", fp);
	fputs("initial.adv\n", fp);
	fclose(fp);

	fp = checked_fopen("initial.adv", "w");
	fputs("\t!RND:0!\n", fp);
	fputs("\t!D01:0!!D02:0!!D03:0!!D04:0!!D05:0!!D06:0!!D07:0!!D08:0!!D09:0!!D10:0!\n", fp);
	fputs("\t!D11:0!!D12:0!!D13:0!!D14:0!!D15:0!!D16:0!!D17:0!!D18:0!!D19:0!!D20:0!\n", fp);
	fputs("\tWW 640, 1440, 24:\n", fp);
	fputs("\tWV 0, 0, 640, 480:\n", fp);
	fputs("\tB1 1, 450, 20, 172, 240, 1:\n", fp);
	fputs("\tB2 1, 1, 0, 0, 0, 0:\n", fp);
	fputs("\tB3 1, 200, 380, 430, 90, 0:\n", fp);
	fputs("\tB4 1, 1, 0, 0, 1, 0:\n", fp);
	fclose(fp);

	return 0;
}
