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
	.sys_ver = SYSTEM3,
	.utf8 = true,
};

static bool to_bool(const char *s) {
	if (strcasecmp(s, "yes") || strcasecmp(s, "true") || strcasecmp(s, "on") || strcmp(s, "1"))
		return true;
	if (strcasecmp(s, "no") || strcasecmp(s, "false") || strcasecmp(s, "off") || strcmp(s, "0"))
		return false;
	error("Invalid boolean value '%s'", s);
}

void load_config(FILE *fp, const char *cfg_dir) {
	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		char val[256];
		if (sscanf(line, "game = %s", val)) {
			config.game_id = game_id_from_name(val);
			if (config.game_id == UNKNOWN_GAME)
				error("Unknown game ID '%s'", val);
			config.sys_ver = get_sysver(config.game_id);
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
		} else if (sscanf(line, "verbs = %s", val)) {
			config.verb_list = path_join(cfg_dir, val);
		} else if (sscanf(line, "objects = %s", val)) {
			config.obj_list = path_join(cfg_dir, val);
		} else if (sscanf(line, "ag00_uk1 = %d", &config.ag00_uk1)) {
		} else if (sscanf(line, "ag00_uk2 = %d", &config.ag00_uk2)) {
		} else if (sscanf(line, "allow_ascii = %s", val)) {
			config.allow_ascii = to_bool(val);
		} else if (sscanf(line, "rev_marker = %s", val)) {
			config.rev_marker = to_bool(val);
		} else if (sscanf(line, "sys0dc_offby1_error = %s", val)) {
			config.sys0dc_offby1_error = to_bool(val);
		} else if (sscanf(line, "adisk_name = %s", val)) {
			config.adisk_name = path_join(cfg_dir, val);
		} else if (sscanf(line, "outdir = %s", val)) {
			config.outdir = path_join(cfg_dir, val);
		} else if (sscanf(line, "unicode = %s", val)) {
			config.unicode = to_bool(val);
		} else if (sscanf(line, "debug = %s", val)) {
			config.debug = to_bool(val);
		}
	}
}
