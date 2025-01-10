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
	AG00 *ag00;
	Vector *variables;
	bool non_unique_verbs[256];
	bool non_unique_objs[256];
	FILE *out;

	int page;
	const uint8_t *p;  // Points inside scos->data[page]->data
	int indent;

	bool quoted_strings;
	bool rev_marker;
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
	STRING_SYSENG = 1 << 2,  // single-quotes are escaped in input
};

static void dc_put_string(const char *s, int len, unsigned flags) {
	if (!dc.out)
		return;

	const char *end = s + len;
	while (s < end) {
		uint8_t c = *s++;
		if (isgraph(c) || c == '\t') {
			if (flags & STRING_SYSENG && c == '\\' && *s == '\'') {
				dc_putc(c);
				c = *s++;
			} else if (flags & STRING_ESCAPE && (c == '\\' || c == '\'' || c == '"' || c == '<')) {
				dc_putc('\\');
			}
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
			} else if ((flags & STRING_EXPAND) && compact_sjis(c, c2)) {
				// Fukei has some uncompacted characters. Emit them as character references.
				dc_printf("<0x%04X>", c << 8 | c2);
			} else {
				dc_putc(c);
				dc_putc(c2);
			}
		}
	}
}

static const void *decompile_syseng_string(const char *s) {
	const char *end;
	for (end = s; *end != '\''; end++) {
		if (end[0] == '\\' && end[1] == '\'')
			end++;
	}
	dc_put_string(s, end - s, STRING_ESCAPE | STRING_SYSENG);
	return end + 1;
}

static const void *decompile_string_arg(const char *s) {
	const char *end = strchr(s, ':');
	assert(end);
	for (const char *p = s; p < end; p++) {
		if ((p == s && *p == ' ') || *p == ',') {
			dc_putc('"');
			dc_put_string(s, end - s, STRING_ESCAPE);
			dc_putc('"');
			return end + 1;
		}
	}
	dc_put_string(s, end - s, 0);
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
		if (cmd != '%' || page != 0) {
			Sco *sco = page < dc.scos->len ? dc.scos->data[page] : NULL;
			if (sco) {
				fprintf(dc.out, "#%s", sco->src_name);
				return;
			} else {
				warning_at(dc.p, "%s to non-existent page %d", cmd == '%' ? "call" : "jump", page + 1);
			}
		}
	}
	print_cali(node, dc.variables, dc.out);
}

static void label(void) {
	uint16_t addr = le16(dc.p);
	dc.p += 2;
	if (addr == 0) {
		dc_putc('0');
		return;
	}
	dc_printf("L_%05x", addr);

	uint8_t *mark = mark_at(dc.page, addr);
	if (!*mark && addr < dc_addr())
		current_sco()->needs_reanalysis = true;
	*mark |= LABEL;
}

static void verb_obj(void) {
	uint8_t verb = *dc.p++;
	uint8_t obj = *dc.p++;
	if (!dc.ag00)
		error("AG00.DAT is required to decompile this file");
	if (verb >= dc.ag00->verbs->len)
		error("invalid verb %d", verb);
	if (obj >= dc.ag00->objs->len)
		error("invalid obj %d", obj);

	label();
	dc_puts(", ");

	if (dc.non_unique_verbs[verb])
		dc_printf("/* %s */ %d", dc.ag00->verbs->data[verb], verb);
	else
		dc_printf("\"%s\"", dc.ag00->verbs->data[verb]);

	if (obj) {
		dc_puts(", ");
		if (dc.non_unique_objs[obj])
			dc_printf("/* %s */ %d", dc.ag00->objs->data[obj], obj);
		else
			dc_printf("\"%s\"", dc.ag00->objs->data[obj]);
	}
	dc_putc(':');
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

static bool is_branch_end(int addr, Vector *branch_end_stack) {
	return branch_end_stack->len > 0 && stack_top(branch_end_stack) == addr;
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
			if (*dc.p == '\'') {  // SysEng-style string
				dc.quoted_strings = true;
				dc.p++;
				dc_putc('"');
				dc.p = decompile_syseng_string((const char *)dc.p);
				dc_putc('"');
			} else {
				dc.p = decompile_string_arg((const char *)dc.p);
			}
			break;
		default:
			error("BUG: invalid arguments() template : %c", *sig);
		}
	}
	dc_putc(':');
}

static void arguments_by_sysver(const char *sig1, const char *sig2, const char *sig3, const char *sig3t) {
	switch (config.sys_ver) {
	case SYSTEM1: arguments(sig1); break;
	case SYSTEM2: arguments(sig2); break;
	case SYSTEM3: arguments(config.game_id < TOUSHIN2 ? sig3 : sig3t); break;
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

	dc_put_string((const char *)dc.p, end - dc.p, STRING_EXPAND);
	dc.p = end;
	dc_putc(*dc.p++);  // '$'
	return true;
}

static void decompile_page(int page) {
	Sco *sco = dc.scos->data[page];
	dc.page = page;
	dc.p = sco->data + 2;
	dc.indent = 1;
	bool in_menu_item = false;
	bool default_label_defined = false;
	Vector *branch_end_stack = new_vec();

	// Skip the "REV" marker for old SysEng.
	if (page == 0 && !memcmp(dc.p, "REV", 3)) {
		dc.rev_marker = true;
		dc.p += 3;
	}

	while (dc.p < sco->data + sco->filesize) {
		int topaddr = dc.p - sco->data;
		uint8_t mark = sco->mark[dc.p - sco->data];
		while (is_branch_end(topaddr, branch_end_stack)) {
			stack_pop(branch_end_stack);
			dc.indent--;
			assert(dc.indent > 0);
			indent();
			dc_puts("}\n");
		}
		if (dc.p - sco->data == sco->default_addr) {
			print_address();
			dc_printf("*default:\n");
			default_label_defined = true;
		}
		if (mark & LABEL) {
			print_address();
			dc_printf("*L_%05x:\n", dc.p - sco->data);
		}

		if (*dc.p == '>' || *dc.p == '}')
			dc.indent--;
		indent();
		if (*dc.p == 0x20 || *dc.p > 0x80) {
			sco->mark[dc.p - sco->data] |= CODE;
			dc_putc('\'');
			const uint8_t *begin = dc.p;
			while (*dc.p == 0x20 || *dc.p > 0x80) {
				dc.p = advance_char(dc.p);
				if (dc.p >= sco->data + sco->filesize || *mark_at(dc.page, dc_addr()) != 0)
					break;
			}
			dc_put_string((const char *)begin, dc.p - begin, STRING_ESCAPE | STRING_EXPAND);
			dc_putc('\'');
			// Print subsequent R/A command on the same line if possible.
			if ((*dc.p == 'R' || *dc.p == 'A') &&
				!is_branch_end(dc_addr(), branch_end_stack) &&
				!(sco->mark[dc.p - sco->data] & ~CODE)) {
				dc_putc(*dc.p++);
			}
			dc_putc('\n');
			continue;
		}
		if (*dc.p == 0 || *dc.p == 0x1a) {
			int len = 1;
			while (dc_addr() + len < sco->filesize && !sco->mark[dc_addr() + len]) {
				len++;
			}
			while (len > 0 && *dc.p == 0x1a) {
				dc_puts("EOF\n");
				dc.p++;
				if (--len > 0)
					indent();
			}
			if (len > 0) {
				dc_putc('"');
				while (len-- > 0)
					dc_printf("\\x%02x", *dc.p++);
				dc_puts("\" ; Junk data (can be removed safely)\n");
			}
			continue;
		}
		if (*dc.p == 0x7f) {
			// Hack for page 23 address 0xe2 of Abunai Tengu Densetsu.
			dc_putc('"');
			while (*dc.p == 0x7f || *dc.p == 0x40)
				dc_printf("\\x%02x", *dc.p++);
			dc_puts("\"\n");
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
			dc.quoted_strings = true;
			dc.p = decompile_syseng_string((const char *)dc.p);
			dc_putc('\'');
			break;

		case 'A': break;
		case 'B': arguments_by_sysver(NULL, "eeeeeee", "neeeeee", "ee"); break;
		case 'D':
			arguments_by_sysver(NULL, config.game_id >= RANCE4_OPT ? "eee" : "eeeeeeee", NULL, NULL);
			break;
		case 'E': arguments_by_sysver(NULL, "eee", "eeeeee", "eeeeee"); break;
		case 'F': break;
		case 'G': arguments_by_sysver("n", "e", "e", "e"); break;
		case 'H': arguments_by_sysver(NULL, "ne", "ne", "ne"); break;
		case 'I': arguments_by_sysver(NULL, config.game_id < DALK ? "een" : "eee", "eeeeee", "eee"); break;
		case 'J': arguments_by_sysver(NULL, "ee", "ee", "ee"); break;
		case 'K': arguments_by_sysver(NULL, "", "n", "n"); break;
		case 'L': arguments_by_sysver("n", "n", "e", "e"); break;
		case 'M': arguments_by_sysver(NULL, "s", "s", "s"); break;
		case 'N': arguments_by_sysver(NULL, "ee", "nee", "ee"); break;
		case 'O': arguments_by_sysver(NULL, "eee", "ev", "ev"); break;
		case 'P': arguments_by_sysver("n", "n", "eeee", "e"); break;
		case 'Q': arguments_by_sysver("n", "n", "e", "e"); break;
		case 'R': break;
		case 'S': arguments("n"); break;
		case 'T': arguments_by_sysver(NULL, "eee", "ee", "eee"); break;
		case 'U':
			arguments_by_sysver("nn", config.game_id < RANCE3 ? "nn" : "ee", "ee", "ee");
			break;
		case 'V': arguments_by_sysver(NULL, "neeeeeeeeeeeeeeeeeeeeeeeeeeeee", "ee", "ee"); break;
		case 'W': arguments_by_sysver(NULL, "eeee", "eee", "eee"); break;
		case 'X': arguments("n"); break;
		case 'Y': arguments("ee"); break;
		case 'Z': arguments_by_sysver("ee", "ee", "ee", "eee"); break;
		default:
			error("%s:%x: unknown command '%.*s'", sjis2utf(sco->sco_name), topaddr, dc_addr() - topaddr, sco->data + topaddr);
		}
		dc_putc('\n');
	}
	int eofaddr = dc.p - sco->data;
	while (branch_end_stack->len > 0 && stack_top(branch_end_stack) == eofaddr) {
		stack_pop(branch_end_stack);
		dc.indent--;
		assert(dc.indent > 0);
		indent();
		dc_puts("}\n");
	}
	if (sco->mark[eofaddr] & LABEL)
		dc_printf("*L_%05x:\n", eofaddr);
	if (!default_label_defined)
		dc_printf("pragma default_address 0x%04x:\n", sco->default_addr);
}

static void write_config(const char *path, const char *adisk_name) {
	if (dc.scos->len == 0)
		return;
	FILE *fp = checked_fopen(path, "w+");
	fprintf(fp, "game = %s\n", game_id_to_name(config.game_id));
	if (adisk_name)
		fprintf(fp, "adisk_name = %s\n", adisk_name);

	fputs("hed = sys3dc.hed\n", fp);
	fputs("variables = variables.txt\n", fp);
	if (dc.ag00) {
		fputs("verbs = verbs.txt\n", fp);
		fputs("objects = objects.txt\n", fp);
		fprintf(fp, "ag00_uk1 = %d\n", dc.ag00->uk1);
		fprintf(fp, "ag00_uk2 = %d\n", dc.ag00->uk2);
	}
	if (dc.quoted_strings)
		fputs("quoted_strings = true\n", fp);
	if (dc.rev_marker)
		fputs("rev_marker = true\n", fp);
	if (sys0dc_offby1_error)
		fputs("sys0dc_offby1_error = true\n", fp);

	fprintf(fp, "encoding = %s\n", config.utf8_output ? "utf8" : "sjis");
	if (config.utf8_input)
		fprintf(fp, "unicode = true\n");

	fclose(fp);
}

static void write_hed(const char *path) {
	FILE *fp = checked_fopen(path, "w+");
	for (int i = 0; i < dc.scos->len; i++) {
		Sco *sco = dc.scos->data[i];
		fprintf(fp, "%s\n", sco ? sco->src_name : "");
	}

	if (!config.utf8_input && config.utf8_output)
		convert_to_utf8(fp);
	fclose(fp);
}

static void write_txt(const char *path, Vector *lines) {
	FILE *fp = checked_fopen(path, "w+");
	for (int i = 0; i < lines->len; i++) {
		const char *s = lines->data[i];
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

static void find_duplicates(Vector *list, bool *duplicates) {
	for (int i = 0; i < list->len; i++) {
		const char *s = list->data[i];
		for (int j = i + 1; j < list->len; j++) {
			if (!strcmp(s, list->data[j])) {
				duplicates[i] = duplicates[j] = true;
			}
		}
	}
}

void decompile(Vector *scos, AG00 *ag00, const char *outdir, const char *adisk_name) {
	memset(&dc, 0, sizeof(dc));
	dc.scos = scos;
	if (ag00) {
		dc.ag00 = ag00;
		find_duplicates(ag00->verbs, dc.non_unique_verbs);
		find_duplicates(ag00->objs, dc.non_unique_objs);
	}
	dc.variables = new_vec();
	vec_push(dc.variables, "RND");
	if (config.sys_ver == SYSTEM3) {
		char buf[4];
		for (int i = 1; i <= 20; i++) {
			sprintf(buf, "D%02d", i);
			vec_push(dc.variables, strdup(buf));
		}
		for (int i = 1; i <= 20; i++) {
			sprintf(buf, "U%02d", i);
			vec_push(dc.variables, strdup(buf));
		}
		for (int i = 1; i <= 16; i++) {
			sprintf(buf, "B%02d", i);
			vec_push(dc.variables, strdup(buf));
		}
		vec_push(dc.variables, "M_X");
		vec_push(dc.variables, "M_Y");
	}

	// Analyze
	for (int i = 0; i < scos->len; i++) {
		Sco *sco = scos->data[i];
		if (!sco)
			continue;
		if (config.verbose)
			printf("Analyzing %s (page %d)...\n", sjis2utf(sco->sco_name), i);
		do {
			sco->needs_reanalysis = false;
			decompile_page(i);
		} while (sco->needs_reanalysis);
	}

	// Decompile
	for (int i = 0; i < scos->len; i++) {
		Sco *sco = scos->data[i];
		if (!sco)
			continue;
		if (config.verbose)
			printf("Decompiling %s (page %d)...\n", sjis2utf(sco->sco_name), i);
		dc.out = checked_fopen(path_join(outdir, to_utf8(sco->src_name)), "w+");
		if (sco->volume_bits != 1 << 1) {
			fputs("pragma dri_volume ", dc.out);
			for (int v = 1; v <= DRI_MAX_VOLUME; v++) {
				if (sco->volume_bits & (1 << v))
					fputc(v + 'A' - 1, dc.out);
			}
			fputs(":\n", dc.out);
		}
		decompile_page(i);
		if (!config.utf8_input && config.utf8_output)
			convert_to_utf8(dc.out);
		fclose(dc.out);
	}

	if (config.verbose)
		puts("Generating config files...");

	write_config(path_join(outdir, "sys3c.cfg"), adisk_name);
	write_hed(path_join(outdir, "sys3dc.hed"));
	write_txt(path_join(outdir, "variables.txt"), dc.variables);
	if (ag00) {
		write_txt(path_join(outdir, "verbs.txt"), ag00->verbs);
		write_txt(path_join(outdir, "objects.txt"), ag00->objs);
	}

	if (config.verbose)
		puts("Done!");
}
