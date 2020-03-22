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

static void ain_emit_HEL0(Buffer *out) {
	emit(out, 'H');
	emit(out, 'E');
	emit(out, 'L');
	emit(out, '0');
	emit_dword(out, 0);  // reserved
	emit_dword(out, 0);  // number of DLLs
}

static void ain_emit_FUNC(Buffer *out, Map *functions) {
	emit(out, 'F');
	emit(out, 'U');
	emit(out, 'N');
	emit(out, 'C');
	emit_dword(out, 0);  // reserved
	emit_dword(out, functions->keys->len);
	for (int i = 0; i < functions->keys->len; i++) {
		emit_string(out, functions->keys->data[i]);
		emit(out, 0);
		Function *f = functions->vals->data[i];
		emit_word(out, f->page);
		emit_dword(out, f->addr);
	}
}

static void ain_emit_VARI(Buffer *out, Vector *variables) {
	emit(out, 'V');
	emit(out, 'A');
	emit(out, 'R');
	emit(out, 'I');
	emit_dword(out, 0);  // reserved
	emit_dword(out, variables->len);
	for (int i = 0; i < variables->len; i++) {
		emit_string(out, variables->data[i]);
		emit(out, 0);
	}
}

static void ain_emit_MSGI_head(Buffer *out, int msg_count) {
	emit(out, 'M');
	emit(out, 'S');
	emit(out, 'G');
	emit(out, 'I');
	emit_dword(out, 0);  // reserved
	emit_dword(out, msg_count);
}

static void ain_write_buf(Buffer *buf, FILE *fp) {
	uint8_t *end = buf->buf + buf->len;
	for (uint8_t *p = buf->buf; p < end; p++)
		fputc(*p >> 2 | *p << 6, fp);
}

void ain_write(Compiler *compiler, FILE *fp) {
	fputs("AINI", fp);
	Buffer *out = new_buf();
	emit_dword(out, 4);  // number of sections
	ain_emit_HEL0(out);
	ain_emit_FUNC(out, compiler->functions);
	ain_emit_VARI(out, compiler->variables);
	ain_emit_MSGI_head(out, compiler->msg_count);
	ain_write_buf(out, fp);
	ain_write_buf(compiler->msg_buf, fp);
}
