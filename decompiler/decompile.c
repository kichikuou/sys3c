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
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Config config = {
	.sys_ver = SYSTEM3,
	.utf8_output = true,
};

typedef struct {
	Vector *scos;
	Vector *variables;
	FILE *out;

	int page;
	const uint8_t *p;  // Points inside scos->data[page]->data
	int indent;

	bool ascii_messages;
} Decompiler;

static Decompiler dc;

static inline Sco *current_sco(void) {
	return dc.scos->data[dc.page];
}

static inline int dc_addr(void) {
	return dc.p - current_sco()->data;
}

static uint8_t *mark_at(int page, int addr) {
	if (page >= dc.scos->len)
		error("page out of range (%x:%x)", page, addr);
	Sco *sco = dc.scos->data[page];
	if (!sco)
		error("page does not exist (%x:%x)", page, addr);
	if (addr > sco->filesize)
		error("address out of range (%x:%x)", page, addr);
	return &sco->mark[addr];
}

static const uint8_t *advance_char(const uint8_t *s) {
	if (config.utf8_input) {
		while (UTF8_TRAIL_BYTE(*++s))
			;
	} else {
		s += is_sjis_byte1(*s) ? 2 : 1;
	}
	return s;
}

static void dc_putc(int c) {
	if (dc.out)
		fputc(c, dc.out);
}

static void dc_puts(const char *s) {
	if (dc.out)
		fputs(s, dc.out);
}

static void dc_printf(const char *fmt, ...) {
	if (!dc.out)
		return;
	va_list args;
	va_start(args, fmt);
	vfprintf(dc.out, fmt, args);
}

enum dc_put_string_flags {
	STRING_ESCAPE = 1 << 0,
	STRING_EXPAND = 1 << 1,
};

static void dc_put_string_n(const char *s, int len, unsigned flags) {
	if (!dc.out)
		return;

	const char *end = s + len;
	while (s < end) {
		uint8_t c = *s++;
		if (isgraph(c) || c == '\t') {
			if (flags & STRING_ESCAPE && (c == '\\' || c == '\'' || c == '"' || c == '<'))
				dc_putc('\\');
			dc_putc(c);
		} else if (config.utf8_input) {
			dc_putc(c);
			while (UTF8_TRAIL_BYTE(*s))
				dc_putc(*s++);
		} else if (is_compacted_sjis(c)) {
			if (flags & STRING_EXPAND) {
				uint16_t full = expand_sjis(c);
				dc_putc(full >> 8);
				dc_putc(full & 0xff);
			} else {
				dc_putc(c);
			}
		} else if (c == 0xde || c == 0xdf) {  // Halfwidth (semi-)voiced sound mark
			dc_putc(c);
		} else {
			assert(is_sjis_byte1(c));
			uint8_t c2 = *s++;
			if (config.utf8_output && (flags & STRING_ESCAPE) && !is_unicode_safe(c, c2)) {
				dc_printf("<0x%04X>", c << 8 | c2);
			} else {
				dc_putc(c);
				dc_putc(c2);
			}
		}
	}
}

static const void *dc_put_string(const char *s, int terminator, unsigned flags) {
	const char *end = strchr(s, terminator);
	assert(end);
	dc_put_string_n(s, end - s, flags);
	return end + 1;
}

static void print_address(void) {
	if (config.address)
		dc_printf("/* %05x */\t", dc_addr());
}

static void indent(void) {
	if (!dc.out)
		return;
	print_address();
	for (int i = 0; i < dc.indent; i++)
		fputc('\t', dc.out);
}

static Cali *cali(bool is_lhs) {
	Cali *node = parse_cali(&dc.p, is_lhs);
	if (dc.out)
		print_cali(node, dc.variables, dc.out);
	return node;
}

static void page_name(int cmd) {
	Cali *node = parse_cali(&dc.p, false);
	if (!dc.out)
		return;
	if (node->type == NODE_NUMBER) {
		int page = node->val;
		if ((cmd != '%' || page != 0) && page < dc.scos->len) {
			Sco *sco = dc.scos->data[page];
			if (sco) {
				fprintf(dc.out, "#%s", sco->src_name);
				return;
			}
		}
	}
	print_cali(node, dc.variables, dc.out);
}

static void label(void) {
	uint16_t addr = le16(dc.p);
	dc.p += 2;
	if (addr == 0)
		dc_putc('0');
	else
		dc_printf("L_%05x", addr);

	uint8_t *mark = mark_at(dc.page, addr);
	if (!(*mark & LABEL) && addr < dc_addr())
		current_sco()->analyzed = false;
	*mark |= LABEL;
}

static void verb_obj(void) {
	uint8_t verb = *dc.p++;
	uint8_t obj = *dc.p++;
	label();
	dc_printf(", %d, %d:", verb, obj);
}

static void conditional(Vector *branch_end_stack) {
	dc.indent++;
	cali(false);
	dc_putc(':');

	if (config.sys_ver == SYSTEM3) {
		uint16_t endaddr = le16(dc.p);
		dc.p += 2;
		*mark_at(dc.page, endaddr) |= CODE;
		stack_push(branch_end_stack, endaddr);
	}
}

// Decompile command arguments. Directives:
//  e: expression
//  n: number (single-byte)
//  s: string (colon-terminated)
//  v: variable
static void arguments(const char *sig) {
	if (!sig)
		error_at(dc.p, "invalid command");

	const char *sep = " ";
	for (; *sig; sig++) {
		dc_puts(sep);
		sep = ", ";

		switch (*sig) {
		case 'e':
		case 'v':
			cali(false);
			break;
		case 'n':
			dc_printf("%d", *dc.p++);
			break;
		case 's':
			dc.p = dc_put_string((const char *)dc.p, ':', 0);
			break;
		default:
			error("BUG: invalid arguments() template : %c", *sig);
		}
	}
	dc_putc(':');
}

static void arguments_by_sysver(const char *sig1, const char *sig2, const char *sig3) {
	switch (config.sys_ver) {
	case SYSTEM1: arguments(sig1); break;
	case SYSTEM2: arguments(sig2); break;
	case SYSTEM3: arguments(sig3); break;
	}
}

static int get_command(void) {
	dc_putc(*dc.p++);
	return dc.p[-1];
}

static bool inline_menu_string(void) {
	const uint8_t *end = dc.p;
	while (*end == 0x20 || *end > 0x80)
		end = advance_char(end);
	if (*end != '$')
		return false;

	dc_put_string_n((const char *)dc.p, end - dc.p, STRING_EXPAND);
	dc.p = end;
	dc_putc(*dc.p++);  // '$'
	return true;
}

static void decompile_page(int page) {
	Sco *sco = dc.scos->data[page];
	dc.page = page;
	dc.p = sco->data + sco->hdrsize;
	dc.indent = 1;
	bool in_menu_item = false;
	Vector *branch_end_stack = new_vec();

	// Skip the "ZU 1:" command of unicode SCO.
	if (config.utf8_input && page == 0 && !memcmp(dc.p, "ZU\x41\x7f", 4))
		dc.p += 4;

	while (dc.p < sco->data + sco->filesize && *dc.p != 0x1a) {
		int topaddr = dc.p - sco->data;
		uint8_t mark = sco->mark[dc.p - sco->data];
		while (branch_end_stack->len > 0 && stack_top(branch_end_stack) == topaddr) {
			stack_pop(branch_end_stack);
			dc.indent--;
			assert(dc.indent > 0);
			indent();
			dc_puts("}\n");
		}
		if (mark & LABEL) {
			print_address();
			dc_printf("*L_%05x:\n", dc.p - sco->data);
		}

		if (*dc.p == '>' || *dc.p == '}')
			dc.indent--;
		indent();
		if (*dc.p == 0 || *dc.p == 0x20 || *dc.p > 0x80) {
			sco->mark[dc.p - sco->data] |= CODE;
			dc_putc('\'');
			const uint8_t *begin = dc.p;
			while (*dc.p == 0x20 || *dc.p > 0x80) {
				dc.p = advance_char(dc.p);
				if (*mark_at(dc.page, dc_addr()) != 0)
					break;
			}
			dc_put_string_n((const char *)begin, dc.p - begin, STRING_ESCAPE | STRING_EXPAND);
			dc_puts("'\n");
			continue;
		}
		sco->mark[dc.p - sco->data] |= CODE;
		int cmd = get_command();
		switch (cmd) {
		case '!':  // Assign
			cali(true);
			dc_putc(' ');
			dc_puts(": ");
			cali(false);
			dc_putc('!');
			break;

		case '{':  // Branch
			conditional(branch_end_stack);
			break;

		case '}':  // Branch end
			break;

		case '@':  // Label jump
			label();
			dc_putc(':');
			break;

		case '\\': // Label call
			label();
			dc_putc(':');
			break;

		case '&':  // Page jump
			page_name(cmd);
			dc_putc(':');
			break;

		case '%':  // Page call / return
			page_name(cmd);
			dc_putc(':');
			break;

		case '[':  // Verb-obj
			verb_obj();
			break;

		case ':':  // Conditional verb-obj
			cali(false);
			dc_puts(", ");
			verb_obj();
			break;

		case ']':  // Menu
			break;

		case '$':  // Menu item
			in_menu_item = !in_menu_item;
			if (in_menu_item) {
				label();
				dc_putc('$');
				if (inline_menu_string())
					in_menu_item = false;
			}
			break;

		case '\'':  // SysEng-style message
			dc.ascii_messages = true;
			const uint8_t *begin = dc.p;
			while (*dc.p != '\'') {
				if (*dc.p == '\\')
					dc.p++;
				dc.p = advance_char(dc.p);
			}
			dc_put_string_n((const char *)begin, dc.p - begin, STRING_ESCAPE);
			dc_putc(*dc.p++);  // '\''
			break;

		case 'A': break;
		case 'B': arguments_by_sysver(NULL, "eeeeeee", "neeeeee"); break;
		case 'E': arguments_by_sysver(NULL, "eee", "eeeeee"); break;
		case 'F': break;
		case 'G': arguments_by_sysver("n", "e", "e"); break;
		case 'H': arguments_by_sysver(NULL, "ne", "ne"); break;
		case 'I': arguments_by_sysver(NULL, "eee", "eeeeee"); break;
		case 'J': arguments_by_sysver(NULL, "ee", "ee"); break;
		case 'K': arguments_by_sysver(NULL, NULL, "n"); break;
		case 'L': arguments_by_sysver("n", "n", "e"); break;
		case 'M': arguments_by_sysver(NULL, "s", "s"); break;
		case 'N': arguments_by_sysver(NULL, "ee", "nee"); break;
		case 'O': arguments_by_sysver(NULL, "eee", "ev"); break;
		case 'P': arguments_by_sysver("n", "n", "eeee"); break;
		case 'Q': arguments_by_sysver("n", "n", "e"); break;
		case 'R': break;
		case 'S': arguments("n"); break;
		case 'T': arguments_by_sysver(NULL, "eee", "ee"); break;
		case 'U': arguments_by_sysver("nn", "nn", "ee"); break;  // FIXME: Use "ee" for yakata2
		case 'V': arguments_by_sysver(NULL, NULL, "ee"); break;
		case 'W': arguments_by_sysver(NULL, "eeee", "eee"); break;
		case 'X': arguments("n"); break;
		case 'Y': arguments("ee"); break;
		case 'Z': arguments("ee"); break;
		default:
			error("%s:%x: unknown command '%.*s'", sjis2utf(sco->sco_name), topaddr, dc_addr() - topaddr, sco->data + topaddr);
		}
		dc_putc('\n');
	}
	while (branch_end_stack->len > 0 && stack_top(branch_end_stack) == sco->filesize) {
		stack_pop(branch_end_stack);
		dc.indent--;
		assert(dc.indent > 0);
		indent();
		dc_puts("}\n");
	}
	if (sco->mark[sco->filesize] & LABEL)
		dc_printf("*L_%05x:\n", sco->filesize);
}

char *missing_adv_name(int page) {
	char buf[32];
	sprintf(buf, "_missing%d.adv", page);
	return strdup(buf);
}

static void write_config(const char *path, const char *adisk_name) {
	if (dc.scos->len == 0)
		return;
	FILE *fp = checked_fopen(path, "w+");
	fprintf(fp, "sys_ver = %d\n", config.sys_ver);
	if (adisk_name)
		fprintf(fp, "adisk_name = %s\n", adisk_name);

	fputs("hed = sys3dc.hed\n", fp);
	fputs("variables = variables.txt\n", fp);
	if (dc.ascii_messages)
		fputs("ascii_messages = true\n", fp);

	fprintf(fp, "encoding = %s\n", config.utf8_output ? "utf8" : "sjis");
	if (config.utf8_input)
		fprintf(fp, "unicode = true\n");

	fclose(fp);
}

static void write_hed(const char *path) {
	FILE *fp = checked_fopen(path, "w+");
	fputs("#SYSTEM35\n", fp);
	for (int i = 0; i < dc.scos->len; i++) {
		Sco *sco = dc.scos->data[i];
		fprintf(fp, "%s\n", sco ? sco->src_name : missing_adv_name(i));
	}

	if (!config.utf8_input && config.utf8_output)
		convert_to_utf8(fp);
	fclose(fp);
}

static void write_variables(const char *path) {
	FILE *fp = checked_fopen(path, "w+");
	for (int i = 0; i < dc.variables->len; i++) {
		const char *s = dc.variables->data[i];
		fprintf(fp, "%s\n", s ? s : "");
	}
	if (!config.utf8_input && config.utf8_output)
		convert_to_utf8(fp);
	fclose(fp);
}

noreturn void error_at(const uint8_t *pos, char *fmt, ...) {
	Sco *sco = dc.scos->data[dc.page];
	assert(sco->data <= pos);
	assert(pos < sco->data + sco->filesize);;
	fprintf(stderr, "%s:%x: ", sjis2utf(sco->sco_name), (unsigned)(pos - sco->data));
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	exit(1);
}

void warning_at(const uint8_t *pos, char *fmt, ...) {
	Sco *sco = dc.scos->data[dc.page];
	assert(sco->data <= pos);
	assert(pos < sco->data + sco->filesize);;
	fprintf(stderr, "Warning: %s:%x: ", sjis2utf(sco->sco_name), (unsigned)(pos - sco->data));
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
}

void decompile(Vector *scos, const char *outdir, const char *adisk_name) {
	memset(&dc, 0, sizeof(dc));
	dc.scos = scos;
	dc.variables = new_vec();

	// Analyze
	bool done = false;
	while (!done) {
		done = true;
		for (int i = 0; i < scos->len; i++) {
			Sco *sco = scos->data[i];
			if (!sco || sco->analyzed)
				continue;
			if (config.verbose)
				printf("Analyzing %s (page %d)...\n", sjis2utf(sco->sco_name), i);
			done = false;
			sco->analyzed = true;
			decompile_page(i);
		}
	}

	// Decompile
	for (int i = 0; i < scos->len; i++) {
		Sco *sco = scos->data[i];
		if (!sco)
			continue;
		if (config.verbose)
			printf("Decompiling %s (page %d)...\n", sjis2utf(sco->sco_name), i);
		dc.out = checked_fopen(path_join(outdir, to_utf8(sco->src_name)), "w+");
		if (sco->ald_volume != 1)
			fprintf(dc.out, "pragma ald_volume %d:\n", sco->ald_volume);
		decompile_page(i);
		if (!config.utf8_input && config.utf8_output)
			convert_to_utf8(dc.out);
		fclose(dc.out);
	}

	if (config.verbose)
		puts("Generating config files...");

	write_config(path_join(outdir, "sys3c.cfg"), adisk_name);
	write_hed(path_join(outdir, "sys3dc.hed"));
	write_variables(path_join(outdir, "variables.txt"));

	if (config.verbose)
		puts("Done!");
}
