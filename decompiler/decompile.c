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
	.utf8_output = true,
};

static inline void annotate(uint8_t* mark, int type) {
	*mark = (*mark & ~TYPE_MASK) | type;
}

typedef struct {
	Vector *scos;
	Vector *variables;
	FILE *out;

	int page;
	const uint8_t *p;  // Points inside scos->data[page]->data
	int indent;

	bool disable_else;
	bool old_SR;
} Decompiler;

static Decompiler dc;

static inline Sco *current_sco(void) {
	return dc.scos->data[dc.page];
}

static inline int dc_addr(void) {
	return dc.p - current_sco()->data;
}

static const uint8_t *code_at(int page, int addr) {
	if (page >= dc.scos->len)
		error("page out of range (%x:%x)", page, addr);
	Sco *sco = dc.scos->data[page];
	if (addr >= sco->filesize)
		error("address out of range (%x:%x)", page, addr);
	return &sco->data[addr];
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

static bool is_string_data(const uint8_t *begin, const uint8_t *end, bool should_expand) {
	if (*begin == '\0' && begin + 1 == end)
		return true;
	for (const uint8_t *p = begin; p < end;) {
		if (*p == '\0')
			return p - begin >= 2;
		if (config.utf8_input) {
			const uint8_t *next;
			if (*p <= 0x7f) {
				next = p + 1;
			} else if (p[0] <= 0xdf) {
				next = p + 2;
			} else if (p[0] <= 0xef) {
				next = p + 3;
			} else if (p[0] <= 0xf7) {
				next = p + 4;
			} else {
				return false;
			}
			if (next > end)
				return false;
			while (++p < next) {
				if (!UTF8_TRAIL_BYTE(*p))
					return false;
			}
		} else {
			if (is_valid_sjis(p[0], p[1]))
				p += 2;
			else if (isprint(*p) || (should_expand && is_compacted_sjis(*p)))
				p++;
			else
				break;
		}
	}
	return false;
}

static void data_block(const uint8_t *end) {
	if (!dc.out) {
		dc.p = end;
		return;
	}

	bool should_expand = true;
	bool prefer_string = false;

	while (dc.p < end) {
		indent();
		if (is_string_data(dc.p, end, should_expand) ||
			(*dc.p == '\0' && (prefer_string || is_string_data(dc.p + 1, end, should_expand)))) {
			dc_putc('"');
			unsigned flags = STRING_ESCAPE | (should_expand ? STRING_EXPAND : 0);
			dc.p = dc_put_string((const char *)dc.p, '\0', flags);
			dc_puts("\"\n");
			prefer_string = true;
			continue;
		}
		prefer_string = false;

		dc_putc('[');
		const char *sep = "";
		for (; dc.p < end && !is_string_data(dc.p, end, should_expand); dc.p += 2) {
			if (dc.p + 1 == end) {
				warning_at(dc.p, "data block with odd number of bytes");
				dc_printf("%s%db", sep, dc.p[0]);
				dc.p++;
				break;
			} else {
				dc_printf("%s%d", sep, dc.p[0] | dc.p[1] << 8);
			}
			sep = ", ";
		}
		dc_puts("]\n");
	}
}

static uint8_t *get_surrounding_else(Vector *branch_end_stack) {
	if (branch_end_stack->len == 0)
		return NULL;
	uint8_t *mark = mark_at(dc.page, dc_addr() - 6);
	if ((*mark & TYPE_MASK) != ELSE)
		return NULL;
	uint32_t addr = le32(dc.p - 5);
	if (addr != stack_top(branch_end_stack))
		return NULL;
	return mark;
}

static void conditional(Vector *branch_end_stack) {
	uint8_t *surrounding_else = get_surrounding_else(branch_end_stack);

	dc.indent++;
	cali(false);
	dc_putc(':');
	uint16_t endaddr = le16(dc.p);
	dc.p += 2;

	*mark_at(dc.page, endaddr) |= CODE;
	const uint8_t *epilogue = code_at(dc.page, endaddr - 5);
	switch (*epilogue) {
	case '@':
		if (!dc.disable_else) {
			uint32_t addr = le32(epilogue + 1);
			if (endaddr <= addr && addr <= current_sco()->filesize) {
				if (surrounding_else && stack_top(branch_end_stack) == addr) {
					stack_pop(branch_end_stack);
					annotate(surrounding_else, ELSE_IF);
				}
				uint8_t *m = mark_at(dc.page, endaddr - 5);
				if ((*m & TYPE_MASK) != ELSE_IF)
					annotate(m, ELSE);
				endaddr = addr;
			} else {
				dc.disable_else = true;
			}
		}
		break;
	case '>':
		break;
	default:
		dc.disable_else = true;
		break;
	}
	stack_push(branch_end_stack, endaddr);
}

static void for_loop(void) {
	uint8_t *mark = mark_at(dc.page, dc_addr()) - 2;
	while (!(*mark & CODE))
		mark--;
	annotate(mark, FOR_START);
	if (*dc.p++ != 0)
		error("for_loop: 0 expected, got 0x%02x", *--dc.p);
	if (*dc.p++ != '<')
		error("for_loop: '<' expected, got 0x%02x", *--dc.p);
	if (*dc.p++ != 1)
		error("for_loop: 1 expected, got 0x%02x", *--dc.p);
	dc.p += 4; // skip label
	parse_cali(&dc.p, false);  // var
	cali(false);  // e2
	dc_puts(", ");
	cali(false);  // e3
	dc_puts(", ");
	cali(false);  // e4
	dc_putc(':');
	dc.indent++;
}

static void loop_end(Vector *branch_end_stack) {
	uint16_t addr = le16(dc.p);
	dc.p += 2;

	uint8_t *mark = mark_at(dc.page, addr);
	Sco *sco = dc.scos->data[dc.page];
	switch (sco->data[addr]) {
	case '{':
		annotate(mark, WHILE_START);
		if (stack_top(branch_end_stack) != dc_addr())
			error("while-loop: unexpected address (%x != %x)", stack_top(branch_end_stack), dc_addr());
		stack_pop(branch_end_stack);
		break;
	case '<':
		break;
	default:
		error("Unexpected loop structure");
	}
}

// Decompile command arguments. Directives:
//  e: expression
//  n: number (single-byte)
//  s: string (colon-terminated)
//  v: variable
//  z: string (zero-terminated)
static void arguments(const char *sig) {
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
		case 'z':
			{
				uint8_t terminator = *sig == 'z' ? 0 : ':';
				dc.p = dc_put_string((const char *)dc.p, terminator, 0);
			}
			break;
		case 'o':  // obfuscated string
			{
				if (*dc.p != 0)
					error_at(dc.p, "0x00 expected");
				dc_putc('"');
				char *buf = strdup((const char *)++dc.p);
				for (uint8_t *p = (uint8_t *)buf; *p; p++)
					*p = *p >> 4 | *p << 4;
				dc_put_string(buf, '\0', 0);
				dc.p += strlen(buf) + 1;
				dc_putc('"');
			}
			break;
		default:
			error("BUG: invalid arguments() template : %c", *sig);
		}
	}
	dc_putc(':');
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

		if (mark & DATA) {
			const uint8_t *data_end = dc.p + 1;
			for (; data_end < sco->data + sco->filesize; data_end++) {
				if (sco->mark[data_end - sco->data] & ~DATA)
					break;
			}
			data_block(data_end);
			continue;
		}
		if ((mark & TYPE_MASK) == ELSE_IF && !dc.disable_else) {
			assert(*dc.p == '@');
			assert(dc.p[5] == '{');
			dc.p += 6;
			dc.indent--;
			stack_pop(branch_end_stack);
			indent();
			dc_puts("} else if {");
			conditional(branch_end_stack);
			dc_putc('\n');
			continue;
		}
		if ((mark & TYPE_MASK) == ELSE && !dc.disable_else) {
			assert(*dc.p == '@');
			dc.p += 5;
			if (le32(dc.p - 4) != dc_addr()) {
				dc.indent--;
				indent();
				dc_puts("} else {\n");
				dc.indent++;
			}
			continue;
		}
		if (*dc.p == '>')
			dc.indent--;
		indent();
		if (*dc.p == 0 || *dc.p == 0x20 || *dc.p > 0x80) {
			uint8_t *mark_at_string_start = &sco->mark[dc.p - sco->data];
			dc_putc('\'');
			const uint8_t *begin = dc.p;
			while (*dc.p == 0x20 || *dc.p > 0x80) {
				dc.p = advance_char(dc.p);
				if (*mark_at(dc.page, dc_addr()) != 0)
					break;
			}
			dc_put_string_n((const char *)begin, dc.p - begin, STRING_ESCAPE | STRING_EXPAND);
			dc_puts("'\n");
			if (*dc.p == '\0') {
				// String data in code area. This happens when the author
				// accidentally use double quotes instead of single quotes.
				*mark_at_string_start |= DATA;
				dc.p++;
			} else {
				*mark_at_string_start |= CODE;
			}
			continue;
		}
		sco->mark[dc.p - sco->data] |= CODE;
		if ((mark & TYPE_MASK) == FOR_START) {
			assert(*dc.p == '!');
			dc.p++;
			dc_putc('<');
			cali(true);
			dc_puts(", ");
			cali(false);
			dc_puts(", ");
			assert(*dc.p == '<');
			dc.p++;
			for_loop();
			dc_putc('\n');
			continue;
		}
		if ((mark & TYPE_MASK) == WHILE_START) {
			assert(*dc.p == '{');
			dc.p++;
			dc_puts("<@");
			conditional(branch_end_stack);
			dc_putc('\n');
			continue;
		}
		int cmd = get_command();
		switch (cmd) {
		case '!':  // Assign
		case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x14: case 0x15: case 0x16: case 0x17:
			{
				cali(true);
				dc_putc(' ');
				if (cmd != '!')
					dc_putc("+-*/%&|^"[cmd - 0x10]);
				dc_puts(": ");
				cali(false);
				dc_putc('!');
			}
			break;

		case '{':  // Branch
			conditional(branch_end_stack);
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

		case '<':  // For-loop
			for_loop();
			break;

		case '>':  // Loop end
			loop_end(branch_end_stack);
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

		case 'A': break;
		case 'B': arguments("neeeeee"); break;
		case 'E': arguments("eeeeee"); break;
		case 'F': break;
		case 'G': arguments("e"); break;
		case 'H': arguments("ne"); break;
		case 'I': arguments("eeeeee"); break;
		case 'J': arguments("ee"); break;
		case 'K': arguments("n"); break;
		case 'L': arguments("e"); break;
		case 'M': arguments("s"); break;
		case 'N': arguments("nee"); break;
		case 'O': arguments("ee"); break;
		case 'P': arguments("eeee"); break;
		case 'Q': arguments("e"); break;
		case 'R': break;
		case 'S': arguments("n"); break;
		case 'T': arguments("ee"); break;
		case 'U': arguments("ee"); break;
		case 'V': arguments("ee"); break;
		case 'W': arguments("eee"); break;
		case 'X': arguments("n"); break;
		case 'Y': arguments("ee"); break;
		case 'Z': arguments("ee"); break;
		default:
			if (dc.out)
				error("%s:%x: unknown command '%.*s'", sjis2utf(sco->sco_name), topaddr, dc_addr() - topaddr, sco->data + topaddr);
			// If we're in the analyze phase, retry as a data block.
			dc.p = sco->data + topaddr;
			sco->mark[topaddr] &= ~CODE;
			sco->mark[topaddr] |= DATA;
			break;
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
	if (adisk_name)
		fprintf(fp, "adisk_name = %s\n", adisk_name);

	fputs("hed = sys3dc.hed\n", fp);
	fputs("variables = variables.txt\n", fp);
	if (dc.disable_else)
		fputs("disable_else = true\n", fp);
	if (dc.old_SR)
		fputs("old_SR = true\n", fp);

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
