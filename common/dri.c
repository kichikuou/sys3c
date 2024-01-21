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

#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _POSIX_MAPPED_FILES
#include <sys/mman.h>
#endif
#ifndef _O_BINARY
#define _O_BINARY 0
#endif

static void write_ptr(int size, int *sector, FILE *fp) {
	*sector += (size + 0xff) >> 8;
	int n = *sector + 1;
	fputc(n & 0xff, fp);
	fputc(n >> 8 & 0xff, fp);
}

static void pad(FILE *fp) {
	// Align to next sector boundary
	long pos = ftell(fp);
	for (; pos & 0xff; pos++)
		fputc(0, fp);
}

static void write_entry(DriEntry *entry, FILE *fp) {
	fwrite(entry->data, entry->size, 1, fp);
}

void dri_write(Vector *entries, int volume, FILE *fp) {
	int sector = 0;

	int ptr_count = 0;
	for (int i = 0; i < entries->len; i++) {
		DriEntry *entry = entries->data[i];
		if (entry && entry->volume_bits & 1 << volume)
			ptr_count++;
	}

	write_ptr((ptr_count + 2) * 2, &sector, fp);
	write_ptr(entries->len * 2 + 1, &sector, fp);
	for (int i = 0; i < entries->len; i++) {
		DriEntry *entry = entries->data[i];
		if (entry && entry->volume_bits & 1 << volume)
			write_ptr(entry->size, &sector, fp);
	}
	pad(fp);

	uint16_t link[DRI_MAX_VOLUME + 1];
	memset(link, 0, sizeof(link));
	for (int i = 0; i < entries->len; i++) {
		DriEntry *entry = entries->data[i];
		int vol = 0;
		if (entry) {
			for (int j = 1; j <= DRI_MAX_VOLUME; j++) {
				if (entry->volume_bits & 1 << j) {
					link[j]++;
					if (!vol || j == volume)
						vol = j;
				}
			}
		}
		fputc(vol, fp);
		fputc(link[vol], fp);
	}
	fputc(0x1a, fp);  // EOF
	pad(fp);

	for (int i = 0; i < entries->len; i++) {
		DriEntry *entry = entries->data[i];
		if (!entry || !(entry->volume_bits & 1 << volume))
			continue;
		write_entry(entry, fp);
		pad(fp);
	}
}

static inline uint8_t *dri_sector(uint8_t *dri, int size, int index) {
	uint8_t *p = dri + index * 2;
	int offset = (p[0] << 8 | p[1] << 16) - 256;
	if (offset > size)
		error("sector offset out of range: %d", offset);
	return dri + offset;
}

static void dri_read_entries(Vector *entries, int volume, uint8_t *data, int dri_size) {
	uint8_t *link_sector = dri_sector(data, dri_size, 0);
	uint8_t *link_sector_end = dri_sector(data, dri_size, 1);

	for (uint8_t *link = link_sector; link < link_sector_end; link += 2) {
		uint8_t vol_nr = link[0];
		uint8_t ptr_nr = link[1];
		if (vol_nr != volume)
			continue;
		uint8_t *entry_ptr = dri_sector(data, dri_size, ptr_nr);
		int entry_size = dri_sector(data, dri_size, ptr_nr + 1) - entry_ptr;
		if (entry_ptr + entry_size > data + dri_size)
			error("entry size exceeds end of dri file");
		int id = (link - link_sector) / 2 + 1;
		DriEntry *e = id <= entries->len ? entries->data[id - 1] : NULL;
		if (e) {
			if (e->size != entry_size || memcmp(e->data, entry_ptr, entry_size))
				error("duplicate entry with different content: %d", id);
			e->volume_bits |= 1 << volume;
		} else {
			e = calloc(1, sizeof(DriEntry));
			e->id = id;
			e->volume_bits = 1 << volume;
			e->data = entry_ptr;
			e->size = entry_size;
			vec_set(entries, id - 1, e);
		}
	}
}

Vector *dri_read(Vector *entries, const char *path) {
	if (!entries)
		entries = new_vec();

	int fd = checked_open(path, O_RDONLY | _O_BINARY);

	struct stat sbuf;
	if (fstat(fd, &sbuf) < 0)
		error("%s: %s", path, strerror(errno));

#ifdef _POSIX_MAPPED_FILES
	uint8_t *p = mmap(NULL, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED)
		error("%s: %s", path, strerror(errno));
#else
	uint8_t *p = malloc(sbuf.st_size);
	size_t bytes = 0;
	while (bytes < sbuf.st_size) {
		ssize_t ret = read(fd, p + bytes, sbuf.st_size - bytes);
		if (ret <= 0)
			error("%s: %s", path, strerror(errno));
		bytes += ret;
	}
#endif
	close(fd);

	int volume = toupper(basename_utf8(path)[0]) - 'A' + 1;
	dri_read_entries(entries, volume, p, sbuf.st_size);

	return entries;
}
