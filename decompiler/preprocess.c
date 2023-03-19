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
#include <string.h>

// Scan the SCO and annotate locations that look like data blocks.
static void scan_for_data_tables(Sco *sco, Vector *scos) {
	const uint8_t *p = sco->data + sco->hdrsize;
	const uint8_t *end = sco->data + sco->filesize - 6;  // -6 for address and cali

	// Scan for the pattern '#' <32-bit address> <cali>
	while (p < end && (p = memchr(p, '#', end - p)) != NULL) {
		uint32_t ptr_addr = le32(++p);
		if (p[5] != 0x7f) // Only check if it is a simple 2-byte cali
			continue;
		if (ptr_addr < sco->hdrsize || ptr_addr > sco->filesize - 4)
			continue;
		// Mark only backward references heuristically. Forward references
		// will be marked in the analyze phase.
		if (ptr_addr < p - sco->data)
			sco->mark[ptr_addr] |= DATA_TABLE;

		uint32_t data_addr = le32(sco->data + ptr_addr);
		if (data_addr >= sco->hdrsize && data_addr < sco->filesize) {
			sco->mark[data_addr] |= DATA;
		}
	}
}

void preprocess(Vector *scos) {
	for (int i = 0; i < scos->len; i++) {
		Sco *sco = scos->data[i];
		if (sco)
			scan_for_data_tables(sco, scos);
	}
}
