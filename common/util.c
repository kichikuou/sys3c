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
#include <errno.h>
#include <iconv.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static iconv_t iconv_s2u = (iconv_t)-1;
static iconv_t iconv_u2s = (iconv_t)-1;

static const uint8_t hankaku81[] = {
	0x20, 0xa4, 0xa1, 0x00, 0x00, 0xa5, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xa2, 0xa3, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t hankaku82[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa7,
	0xb1, 0xa8, 0xb2, 0xa9, 0xb3, 0xaa, 0xb4, 0xab,
	0xb5, 0xb6, 0x00, 0xb7, 0x00, 0xb8, 0x00, 0xb9,
	0x00, 0xba, 0x00, 0xbb, 0x00, 0xbc, 0x00, 0xbd,
	0x00, 0xbe, 0x00, 0xbf, 0x00, 0xc0, 0x00, 0xc1,
	0x00, 0xaf, 0xc2, 0x00, 0xc3, 0x00, 0xc4, 0x00,
	0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0x00, 0x00,
	0xcb, 0x00, 0x00, 0xcc, 0x00, 0x00, 0xcd, 0x00,
	0x00, 0xce, 0x00, 0x00, 0xcf, 0xd0, 0xd1, 0xd2,
	0xd3, 0xac, 0xd4, 0xad, 0xd5, 0xae, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0x00, 0xdc, 0x00, 0x00,
	0xa6, 0xdd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint16_t kanatbl[] = {
	0x8140, 0x8142, 0x8175, 0x8176, 0x8141, 0x8145, 0x82f0, 0x829f,
	0x82a1, 0x82a3, 0x82a5, 0x82a7, 0x82e1, 0x82e3, 0x82e5, 0x82c1,
	0x815b, 0x82a0, 0x82a2, 0x82a4, 0x82a6, 0x82a8, 0x82a9, 0x82ab,
	0x82ad, 0x82af, 0x82b1, 0x82b3, 0x82b5, 0x82b7, 0x82b9, 0x82bb,
	0x82bd, 0x82bf, 0x82c2, 0x82c4, 0x82c6, 0x82c8, 0x82c9, 0x82ca,
	0x82cb, 0x82cc, 0x82cd, 0x82d0, 0x82d3, 0x82d6, 0x82d9, 0x82dc,
	0x82dd, 0x82de, 0x82df, 0x82e0, 0x82e2, 0x82e4, 0x82e6, 0x82e7,
	0x82e8, 0x82e9, 0x82ea, 0x82eb, 0x82ed, 0x82f1, 0x814a, 0x814b
};

noreturn void error(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	exit(1);
}

FILE *checked_fopen(const char *path, const char *mode) {
	FILE *fp = fopen(path, mode);
	if (!fp)
		error("cannot open %s: %s", path, strerror(errno));
	return fp;
}

char *sjis2utf(const char *str) {
	if (iconv_s2u == (iconv_t)-1) {
		iconv_s2u = iconv_open("utf8", "CP932");
		if (iconv_s2u == (iconv_t)-1)
			error("iconv_open(utf8, CP932): %s", strerror(errno));
	}
	size_t ilen = strlen(str);
	char *ip = (char *)str;
	size_t olen = ilen * 3 + 1;
	char *obuf = malloc(olen);
	char *op = obuf;
	if (iconv(iconv_s2u, &ip, &ilen, &op, &olen) == (size_t)-1)
		error("iconv: %s", strerror(errno));
	*op = '\0';
	return obuf;
}

char *utf2sjis(const char *str) {
	if (iconv_u2s == (iconv_t)-1) {
		iconv_u2s = iconv_open("CP932", "utf8");
		if (iconv_u2s == (iconv_t)-1)
			error("iconv_open(CP932, utf8): %s", strerror(errno));
	}
	size_t ilen = strlen(str);
	char *ip = (char *)str;
	size_t olen = ilen + 1;
	char *obuf = malloc(olen);
	char *op = obuf;
	if (iconv(iconv_u2s, &ip, &ilen, &op, &olen) == (size_t)-1)
		error("iconv: %s", strerror(errno));
	*op = '\0';
	return obuf;
}

uint8_t to_sjis_half_kana(uint8_t c1, uint8_t c2) {
	return c1 == 0x81 ? hankaku81[c2 - 0x40] :
		   c1 == 0x82 ? hankaku82[c2 - 0x40] : 0;
}

uint16_t from_sjis_half_kana(uint8_t c) {
	return kanatbl[c - 0xa0];
}

const char *basename(const char *path) {
	char *p = strrchr(path, PATH_SEPARATOR);
	if (!p)
		return path;
	return p + 1;
}

char *dirname(const char *path) {
	char *p = strrchr(path, PATH_SEPARATOR);
	if (!p)
		return ".";
	return strndup(path, p - path);
}

char *path_join(const char *dir, char *path) {
	if (!dir || path[0] == PATH_SEPARATOR)
		return path;
	char *buf = malloc(strlen(dir) + strlen(path) + 2);
	sprintf(buf, "%s%c%s", dir, PATH_SEPARATOR, path);
	return buf;
}

Vector *new_vec(void) {
	Vector *v = malloc(sizeof(Vector));
	v->data = malloc(sizeof(void *) * 16);
	v->cap = 16;
	v->len = 0;
	return v;
}

void vec_push(Vector *v, void *e) {
	if (v->len == v->cap) {
		v->cap *= 2;
		v->data = realloc(v->data, sizeof(void *) * v->cap);
	}
	v->data[v->len++] = e;
}

void vec_set(Vector *v, int index, void *e) {
	while (v->len <= index)
		vec_push(v, NULL);
	v->data[index] = e;
}

Map *new_map(void) {
	Map *m = malloc(sizeof(Map));
	m->keys = new_vec();
	m->vals = new_vec();
	return m;
}

void map_put(Map *m, const char *key, void *val) {
	vec_push(m->keys, (void *)key);
	vec_push(m->vals, val);
}

void *map_get(Map *m, const char *key) {
	for (int i = m->keys->len - 1; i >= 0; i--)
		if (!strcmp(m->keys->data[i], key))
			return m->vals->data[i];
	return NULL;
}
