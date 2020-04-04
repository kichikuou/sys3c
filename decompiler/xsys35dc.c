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
#include "xsys35dc.h"
#include <stdlib.h>
#include <string.h>

Sco *sco_new(const char *name, const uint8_t *data, int len) {
	Sco *sco = calloc(1, sizeof(Sco));
	sco->data = data;
	sco->mark = calloc(1, len);
	sco->sco_name = name;
	if (!memcmp(data, "S350", 4))
		sco->version = SCO_S350;
	else if (!memcmp(data, "153S", 4))
		sco->version = SCO_153S;
	else if (!memcmp(data, "S351", 4))
		sco->version = SCO_S351;
	else if (!memcmp(data, "S360", 4))
		sco->version = SCO_S360;
	else if (!memcmp(data, "S380", 4))
		sco->version = SCO_S380;
	else
		error("%s: unknown SCO signature", sjis2utf(name));
	sco->hdrsize = le32(data + 4);
	sco->filesize = le32(data + 8);
	sco->page = le32(data + 12);
	int namelen = data[16] | data[17] << 8;
	sco->src_name = strndup((char *)data + 18, namelen);

	if (len != sco->filesize) {
		error("%s: unexpected file size in SCO header (expected %d, got %d)",
			  sjis2utf(name), len, sco->filesize);
	}
	return sco;
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf("usage: xsys35dc aldfile\n");
		return 1;
	}
	Vector *scos = ald_read(argv[1]);
	for (int i = 0; i < scos->len; i++) {
		AldEntry *e = scos->data[i];
		scos->data[i] = sco_new(e->name, e->data, e->size);
	}

	decompile(scos);

	return 0;
}
