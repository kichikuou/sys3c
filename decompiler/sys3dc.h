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

typedef struct {
	const uint8_t *data;
	uint8_t *mark;
	uint16_t default_addr;
	uint32_t filesize;
	uint32_t page;
	const char *src_name;
	const char *sco_name;  // in SJIS
	uint32_t volume_bits;
	bool analyzed;
} Sco;

// Sco.mark[i] stores annotation for Sco.data[i].
enum {
	  CODE        = 1 << 0,
	  LABEL       = 1 << 1,
};

// cali.c

typedef struct Cali {
	enum {
		NODE_NUMBER,
		NODE_VARIABLE,
		NODE_OP,
	} type;
	int val;
	struct Cali *lhs, *rhs;
} Cali;

// The returned node is valid until next parse_cali() call.
Cali *parse_cali(const uint8_t **code, bool is_lhs);
void print_cali(Cali *node, Vector *variables, FILE *out);

// decompile.c

typedef struct {
	SysVer sys_ver;
	GameId game_id;
	bool address;
	bool utf8_input;
	bool utf8_output;
	bool verbose;
} Config;

extern Config config;

void decompile(Vector *scos, const char *outdir, const char *adisk_name);
noreturn void error_at(const uint8_t *pos, char *fmt, ...);
void warning_at(const uint8_t *pos, char *fmt, ...);

// sys3dc.c
void convert_to_utf8(FILE *fp);
const char *to_utf8(const char *s);
