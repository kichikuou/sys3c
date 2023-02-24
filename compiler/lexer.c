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
#include "xsys35c.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

const char *input_name;
int input_page;
const char *input_buf;
const char *input;
int input_line;

void warn_at(const char *pos, char *fmt, ...) {
	int line = 1;
	for (const char *begin = input_buf;; line++) {
		const char *end = strchr(begin, '\n');
		if (!end)  // last line
			end = strchr(begin, '\0');
		if (pos <= end) {
			int col = pos - begin;
			fprintf(stderr, "%s line %d column %d: ", input_name, line, col + 1);
			va_list args;
			va_start(args, fmt);
			vfprintf(stderr, fmt, args);
			fputc('\n', stderr);
			fprintf(stderr, "%.*s\n", (int)(end - begin), begin);
			for (const char *p = begin; p < pos; p++)
				fputc(*p == '\t' ? '\t' : ' ', stderr);
			fprintf(stderr, "^\n");
			break;
		}
		if (!*end)
			error("BUG: cannot find error location");
		begin = end + 1;
	}
}

void lexer_init(const char *source, const char *name, int pageno) {
	input_buf = input = source;
	input_name = name;
	input_page = pageno;
	input_line = 1;
}

void skip_whitespaces(void) {
	while (*input) {
		if (*input == '\n') {
			input++;
			input_line++;
		} else if (isspace(*input)) {
			input++;
		} else if (*input == ';' || (*input == '/' && *(input+1) == '/')) {
			input = strchr(input, '\n');
		} else if (*input == '/' && *(input+1) == '*') {
			const char* top = input;
			do {
				input = strchr(input + 2, '*');
				if (!input)
					error_at(top, "unfinished comment");
			} while (*++input != '/');
			input++;
		} else if (input[0] == (char)0xe3 && input[1] == (char)0x80 && input[2] == (char)0x80) {  // CJK IDEOGRAPHIC SPACE
			input += 3;
		} else {
			break;
		}
	}
	return;
}

char next_char(void) {
	skip_whitespaces();
	return *input;
}

bool consume(char c) {
	if (next_char() != c)
		return false;
	input++;
	return true;
}

void expect(char c) {
	if (next_char() != c)
		error_at(input, "'%c' expected", c);
	input++;
}

bool consume_keyword(const char *keyword) {
	skip_whitespaces();
	if (*input != *keyword)
		return false;
	int len = strlen(keyword);
	if (!strncmp(input, keyword, len) && !isalnum(input[len]) && input[len] != '_') {
		input += len;
		return true;
	}
	return false;
}

uint8_t echo(Buffer *b) {
	uint8_t c = *input++;
	emit(b, c);
	return c;
}

static bool is_identifier(uint8_t c) {
	return isalnum(c) || !isascii(c) || c == '_' || c == '.';
}

static bool is_label(uint8_t c) {
	return !isascii(c) || (isgraph(c) && c != '$' && c != ',' && c != ';' && c != ':');
}

static void advance_to_next_char(void) {
	while (UTF8_TRAIL_BYTE(*++input))
		;
}

char *get_identifier(void) {
	skip_whitespaces();
	const char *top = input;
	if (!is_identifier(*top) || isdigit(*top))
		error_at(top, "identifier expected");
	while (is_identifier(*input))
		advance_to_next_char();
	return strndup_(top, input - top);
}

char *get_label(void) {
	skip_whitespaces();
	const char *top = input;
	while (is_label(*input))
		advance_to_next_char();
	if (input == top)
		error_at(top, "label expected");
	return strndup_(top, input - top);
}

char *get_filename(void) {
	const char *top = input;
	while (is_identifier(*input))
		advance_to_next_char();
	if (input == top)
		error_at(top, "file name expected");
	return strndup_(top, input - top);
}

// number ::= [0-9]+ | '0' [xX] [0-9a-fA-F]+ | '0' [bB] [01]+
int get_number(void) {
	if (!isdigit(next_char()))
		error_at(input, "number expected");
	int base = 10;
	if (input[0] == '0' && tolower(input[1]) == 'x') {
		base = 16;
		input += 2;
	} else if (input[0] == '0' && tolower(input[1]) == 'b') {
		base = 2;
		input += 2;
	}
	char *p;
	long n = strtol(input, &p, base);
	input = p;
	return n;
}

static void compile_multibyte_string(Buffer *b, bool compact) {
	if (config.unicode) {
		while (!isascii(*input))
			echo(b);
		return;
	}

	const char *top = input;
	while (!isascii(*input))
		input++;
	if (!b)
		return;
	char *buf = alloca(input - top + 1);
	strncpy(buf, top, input - top);
	buf[input - top] = '\0';
	char *sjis = utf2sjis(buf);
	if (!compact) {
		emit_string(b, sjis);
		return;
	}
	while (*sjis) {
		uint8_t c1 = *sjis++;
		if (!is_sjis_byte1(c1)) {
			emit(b, c1);
			continue;
		}
		uint8_t c2 = *sjis++;
		uint8_t hk = compact_sjis(c1, c2);
		if (hk) {
			emit(b, hk);
		} else {
			emit(b, c1);
			emit(b, c2);
		}
	}
}

void compile_sjis_codepoint(Buffer *b) {
	const char *top = input;
	expect('<');
	int code = get_number();
	if (config.unicode) {
		char buf[3] = { code >> 8, code & 0xff, 0 };
		if (!is_valid_sjis(buf[0], buf[1]))
			error_at(top, "Invalid SJIS code 0x%x", code);
		emit_string(b, sjis2utf(buf));
	} else {
		emit_word_be(b, code);
	}
	expect('>');
}

void compile_string(Buffer *b, char terminator, bool compact, bool forbid_ascii) {
	const char *top = input;
	while (*input != terminator) {
		if (*input == '<') {
			compile_sjis_codepoint(b);
			continue;
		}
		if (*input == '\\')
			input++;
		if (!*input)
			error_at(top, "unfinished string");
		if (!isascii(*input))
			compile_multibyte_string(b, compact);
		else if (forbid_ascii)
			error_at(input, "ASCII characters cannot be used here");
		else {
			if (!b && *input < ' ')
				warn_at(input, "Warning: Control character in string.");
			echo(b);
		}
	}
	expect(terminator);
}

void compile_message(Buffer *b) {
	const char *top = input;
	while (*input && *input != '\'') {
		if (*input == '<') {
			compile_sjis_codepoint(b);
			continue;
		}
		if (*input == '\\')
			input++;
		if (!*input)
			error_at(top, "unfinished message");
		if (isascii(*input)) {
			if (!b && *input < ' ')
				warn_at(input, "Warning: Control character in message.");
			echo(b);
		} else {
			compile_multibyte_string(b, false);
		}
	}
	expect('\'');
	emit(b, 0);
}

void compile_bare_string(Buffer *b) {
	const char *top = input;
	while (*input != ',' && *input != ':') {
		if (!*input)
			error_at(top, "unfinished string argument");
		if (isascii(*input))
			echo(b);
		else
			compile_multibyte_string(b, false);
	}
}

#define ISKEYWORD(s, len, kwd) ((len) == sizeof(kwd) - 1 && !memcmp((s), (kwd), (len)))

int get_command(Buffer *b) {
	const char *command_top = input;

	if (!*input || *input == '}' || *input == '>')
		return *input;
	if (*input == 'A' || *input == 'R')
		return echo(b);
	if (isupper(*input)) {
		int cmd = *input++;
		if (isupper(*input))
			error_at(command_top, "Unknown command %.2s", command_top);

		emit_command(b, cmd);
		return cmd;
	}
	if (islower(*input)) {
		while (isalnum(*++input))
			;
		int len = input - command_top;
		if (ISKEYWORD(command_top, len, "if"))
			return COMMAND_IF;
		if (ISKEYWORD(command_top, len, "const"))
			return COMMAND_CONST;
		if (ISKEYWORD(command_top, len, "pragma"))
			return COMMAND_PRAGMA;
		error_at(command_top, "Unknown command %.*s", len, command_top);
	}
	return *input++;
}
