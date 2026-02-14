/* Copyright (C) 2024 <KichikuouChrome@gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ag00_gets(FILE *fp, char *buf, int len) {
	int i = 0;
	for (;;) {
		int c = fgetc(fp);
		if (c == '\r' || c == EOF)
			break;
		if (c != '\n') {
			if (i + 1 < len) {
				buf[i++] = c;
			}
		}
	}
	buf[i] = '\0';
	return i > 0;
}

AG00 *ag00_read(const char *path) {
	FILE *fp = checked_fopen(path, "rb");
	char buf[64];

	ag00_gets(fp, buf, sizeof(buf));
	unsigned uk1, nr_verbs, nr_objs, uk2;
	if (sscanf(buf, "%u,%u,%u,%u", &uk1, &nr_verbs, &nr_objs, &uk2) != 4)
		error("Invalid AG00 header");
	if (nr_verbs > 256 || nr_objs > 256)
		error("Invalid AG00 data");

	Vector *verbs = new_vec();
	for (unsigned i = 0; i < nr_verbs; i++) {
		if (!ag00_gets(fp, buf, sizeof(buf)))
			error("Invalid AG00 file");
		vec_push(verbs, strdup(buf));
	}

	Vector *objs = new_vec();
	for (unsigned i = 0; i < nr_objs; i++) {
		if (!ag00_gets(fp, buf, sizeof(buf)))
			error("Invalid AG00 file");
		vec_push(objs, strdup(buf));
	}
	fclose(fp);

	AG00 *ag00 = malloc(sizeof(AG00));
	ag00->verbs = verbs;
	ag00->objs = objs;
	ag00->uk1 = uk1;
	ag00->uk2 = uk2;
	ag00->filename = basename_utf8(path);
	return ag00;
}

void ag00_write(AG00 *ag00, const char *path) {
	FILE *fp = checked_fopen(path, "wb");
	fprintf(fp, "%d,%d,%d,%d\r\n", ag00->uk1, ag00->verbs->len, ag00->objs->len, ag00->uk2);
	for (int i = 0; i < ag00->verbs->len; i++) {
		fprintf(fp, "%s\r", (char *)ag00->verbs->data[i]);
	}
	for (int i = 0; i < ag00->objs->len; i++) {
		fprintf(fp, "%s\r", (char *)ag00->objs->data[i]);
	}
	fputc('\x1a', fp);
	fclose(fp);
}
