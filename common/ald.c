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

static void write_entry(AldEntry *entry, FILE *fp) {
	fwrite(entry->data, entry->size, 1, fp);
}

void ald_write(Vector *entries, int volume, FILE *fp) {
	int sector = 0;

	int ptr_count = 0;
	for (int i = 0; i < entries->len; i++) {
		AldEntry *entry = entries->data[i];
		if (entry && entry->volume == volume)
			ptr_count++;
	}

	write_ptr((ptr_count + 2) * 2, &sector, fp);
	write_ptr(entries->len * 2, &sector, fp);
	for (int i = 0; i < entries->len; i++) {
		AldEntry *entry = entries->data[i];
		if (entry && entry->volume == volume)
			write_ptr(entry->size, &sector, fp);
	}
	pad(fp);

	uint16_t link[256];
	memset(link, 0, sizeof(link));
	for (int i = 0; i < entries->len; i++) {
		AldEntry *entry = entries->data[i];
		int vol = entry ? entry->volume : 0;
		fputc(vol, fp);
		if (vol)
			link[vol]++;
		fputc(link[vol], fp);
	}
	pad(fp);

	for (int i = 0; i < entries->len; i++) {
		AldEntry *entry = entries->data[i];
		if (!entry || entry->volume != volume)
			continue;
		write_entry(entry, fp);
		pad(fp);
	}
}

static inline uint8_t *ald_sector(uint8_t *ald, int size, int index) {
	uint8_t *p = ald + index * 2;
	int offset = (p[0] << 8 | p[1] << 16) - 256;
	if (offset > size)
		error("sector offset out of range: %d", offset);
	return ald + offset;
}

static void ald_read_entries(Vector *entries, int volume, uint8_t *data, int size) {
	uint8_t *link_sector = ald_sector(data, size, 0);
	uint8_t *link_sector_end = ald_sector(data, size, 1);

	for (uint8_t *link = link_sector; link < link_sector_end; link += 2) {
		uint8_t vol_nr = link[0];
		uint8_t ptr_nr = link[1];
		if (vol_nr != volume)
			continue;
		uint8_t *entry_ptr = ald_sector(data, size, ptr_nr);
		AldEntry *e = calloc(1, sizeof(AldEntry));
		e->id = (link - link_sector) / 2 + 1;
		e->volume = volume;
		e->data = entry_ptr;
		e->size = ald_sector(data, size, ptr_nr + 1) - entry_ptr;
		if (e->data + e->size > data + size)
			error("entry size exceeds end of ald file");
		vec_set(entries, e->id - 1, e);
	}
}

Vector *ald_read(Vector *entries, const char *path) {
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
	ald_read_entries(entries, volume, p, sbuf.st_size);

	return entries;
}
