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
#include <stdlib.h>
#include <string.h>

Buffer *new_buf(void) {
	Buffer *b = malloc(sizeof(Buffer));
	b->buf = calloc(1, 4096);
	b->cap = 4096;
	b->len = 0;
	return b;
}

void emit(Buffer *b, uint8_t c) {
	if (b->len == b->cap) {
		b->cap *= 2;
		b->buf = realloc(b->buf, b->cap);
	}
	b->buf[b->len++] = c;
}

void emit_word(Buffer *b, uint16_t v) {
	emit(b, v & 0xff);
	emit(b, v >> 8 & 0xff);
}

void emit_word_be(Buffer *b, uint16_t v) {
	emit(b, v >> 8 & 0xff);
	emit(b, v & 0xff);
}

void emit_dword(Buffer *b, uint32_t v) {
	emit(b, v & 0xff);
	emit(b, v >> 8 & 0xff);
	emit(b, v >> 16 & 0xff);
	emit(b, v >> 24 & 0xff);
}

void emit_string(Buffer *b, const char *s) {
	while (*s)
		emit(b, *s++);
}

int current_address(Buffer *b) {
	return b->len;
}

void set_byte(Buffer *b, uint32_t addr, uint8_t val) {
	b->buf[addr] = val;
}

uint8_t get_byte(Buffer *b, uint32_t addr) {
	assert(addr < b->len);
	return b->buf[addr];
}

uint16_t swap_word(Buffer *b, uint32_t addr, uint16_t val) {
	uint8_t *p = &b->buf[addr];
	uint16_t oldval = p[0] | (p[1] << 8);
	p[0] = val & 0xff;
	p[1] = val >> 8 & 0xff;
	return oldval;
}

uint32_t swap_dword(Buffer *b, uint32_t addr, uint32_t val) {
	uint8_t *p = &b->buf[addr];
	uint32_t oldval = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	p[0] = val & 0xff;
	p[1] = val >> 8 & 0xff;
	p[2] = val >> 16 & 0xff;
	p[3] = val >> 24 & 0xff;
	return oldval;
}

void emit_var(Buffer *b, int var_id) {
	if (var_id <= 0x3f) {
		emit(b, var_id + 0x80);
	} else if (var_id <= 0xff) {
		emit(b, 0xc0);
		emit(b, var_id);
	} else if (var_id <= 0x3fff) {
		emit_word_be(b, var_id + 0xc000);
	} else {
		error("emit_var(%d): not implemented", var_id);
	}
}

void emit_number(Buffer *b, int n) {
	int addop = 0;
	while (n > 0x3fff) {
		emit(b, 0x3f);
		emit(b, 0xff);
		n -= 0x3fff;
		addop++;
	}
	if (n <= (config.sys_ver == SYSTEM1 ? 0x37 : 0x36) - config.sys0dc_offby1_error) {
		emit(b, n + 0x40);
	} else {
		emit_word_be(b, n);
	}
	for (int i = 0; i < addop; i++)
		emit(b, OP_ADD);
}

void emit_command(Buffer *b, int cmd) {
	for (int n = cmd; n; n >>= 8)
		emit(b, n & 0xff);
}
