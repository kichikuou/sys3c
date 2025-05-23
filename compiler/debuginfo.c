/* Copyright (C) 2021 <KichikuouChrome@gmail.com>
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
#include <stdlib.h>
#include <string.h>

#define DSYM_VERSION 0

typedef struct {
	int line;
	int addr;
} LineInfo;

typedef struct {
	const char *name;
	int page;
	int addr;
	bool is_local;
} FuncInfo;

typedef struct DebugInfo {
	Map *srcs;
	Buffer *line_section;
	Vector *linemap;
	int nr_files;
	Vector *functions;
} DebugInfo;

struct DebugInfo *new_debug_info(Map *srcs) {
	DebugInfo *di = calloc(1, sizeof(DebugInfo));
	di->srcs = new_map();
	for (int i = 0; i < srcs->keys->len; i++) {
		char *key = srcs->keys->data[i] ? basename_utf8(srcs->keys->data[i]) : "";
		char *val = srcs->vals->data[i] ? srcs->vals->data[i] : "";
		map_put(di->srcs, key, val);
	}
	di->functions = new_vec();
	return di;
}

void debug_init_page(DebugInfo *di, int page) {
	if (!di->line_section) {
		di->line_section = new_buf();
		emit(di->line_section, 'L');
		emit(di->line_section, 'I');
		emit(di->line_section, 'N');
		emit(di->line_section, 'E');
		emit_dword(di->line_section, 0);  // section length (to be filled later)
		emit_dword(di->line_section, 0);  // nr_files (to be filled later)
	}

	assert(page == di->nr_files);
	assert(!di->linemap);
	di->linemap = new_vec();
}

void debug_line_add(DebugInfo *di, int line, int addr) {
	Vector *linemap = di->linemap;

	if (linemap->len > 0) {
		LineInfo *last = linemap->data[linemap->len - 1];
		assert(addr >= last->addr);
		assert(line >= last->line);
		if (addr == last->addr) {
			last->line = line;
			return;
		}
		if (line == last->line)
			return;
	}
	LineInfo *li = calloc(1, sizeof(LineInfo));
	li->line = line;
	li->addr = addr;
	vec_push(linemap, li);
	return;
}

void debug_finish_page(DebugInfo *di) {
	Vector *linemap = di->linemap;
	assert(linemap);

	// Drop the last entry because it points to the end address of the SCO.
	if (linemap->len > 0)
		linemap->len--;

	emit_dword(di->line_section, linemap->len);
	for (int i = 0; i < linemap->len; i++) {
		LineInfo *li = linemap->data[i];
		emit_dword(di->line_section, li->line);
		emit_dword(di->line_section, li->addr);
	}
	di->linemap = NULL;
	di->nr_files++;

	swap_dword(di->line_section, 4, di->line_section->len);
	swap_dword(di->line_section, 8, di->nr_files);
}

static void write_string_array_section(const char *tag, Vector *vec, FILE *fp) {
	int section_len = 12;
	for (int i = 0; i < vec->len; i++)
		section_len += strlen(vec->data[i]) + 1;

	fputs(tag, fp);
	fputdw(section_len, fp);
	fputdw(vec->len, fp);
	for (int i = 0; i < vec->len; i++) {
		fputs(vec->data[i], fp);
		fputc('\0', fp);
	}
}

int funcinfo_compare(const void *a, const void *b) {
	const FuncInfo *fa = *(const FuncInfo **)a;
	const FuncInfo *fb = *(const FuncInfo **)b;
	if (fa->page != fb->page)
		return fa->page - fb->page;
	else
		return fa->addr - fb->addr;
}

void debug_info_write(struct DebugInfo *di, Compiler *compiler, FILE *fp) {
	fputs("DSYM", fp);
	fputdw(DSYM_VERSION, fp);
	fputdw(4, fp);  // nr_sections

	write_string_array_section("SRCS", di->srcs->keys, fp);
	write_string_array_section("SCNT", di->srcs->vals, fp);
	fwrite(di->line_section->buf, di->line_section->len, 1, fp);
	write_string_array_section("VARI", compiler->variables, fp);
}
