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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <time.h>

#define VERSION "0.2.0"

static inline uint32_t le16(const uint8_t *p) {
	return p[0] | p[1] << 8;
}

// util.c

void init(int *argc, char ***argv);
char *strndup_(const char *s, size_t n);
noreturn void error(char *fmt, ...);
FILE *checked_fopen(const char *path_utf8, const char *mode);
int checked_open(const char *path_utf8, int oflag);

char *basename_utf8(const char *path);
char *dirname_utf8(const char *path);
char *path_join(const char *dir, const char *path);
int make_dir(const char *path_utf8);

extern uint16_t fgetw(FILE *fp);
extern uint32_t fgetdw(FILE *fp);
extern void fputw(uint16_t n, FILE *fp);
extern void fputdw(uint32_t n, FILE *fp);

// sjisutf.c

#define sjis2utf(s) sjis2utf_sub((s), -1)
#define utf2sjis(s) utf2sjis_sub((s), -1)
char *sjis2utf_sub(const char *str, int substitution_char);
char *utf2sjis_sub(const char *str, int substitution_char);
uint8_t compact_sjis(uint8_t c1, uint8_t c2);
uint16_t expand_sjis(uint8_t c);
bool is_valid_sjis(uint8_t c1, uint8_t c2);
bool is_unicode_safe(uint8_t c1, uint8_t c2);

// Returns NULL if s is a valid UTF-8 string. Otherwise, returns the first
// invalid character.
const char *validate_utf8(const char *s);

static inline bool is_sjis_half_kana(uint8_t c) {
	return 0xa1 <= c && c <= 0xdf;
}

static inline bool is_compacted_sjis(uint8_t c) {
	return c == ' ' || (0xa1 <= c && c <= 0xdd);
}

static inline bool is_sjis_byte1(uint8_t c) {
	return (0x81 <= c && c <= 0x9f) || (0xe0 <= c && c <= 0xfc);
}

static inline bool is_sjis_byte2(uint8_t c) {
	return 0x40 <= c && c <= 0xfc && c != 0x7f;
}

#define UTF8_TRAIL_BYTE(b) ((int8_t)(b) < -0x40)

// container.c

typedef struct {
	void **data;
	int len;
	int cap;
} Vector;

Vector *new_vec(void);
void vec_push(Vector *v, void *e);
void vec_set(Vector *v, int index, void *e);

void stack_push(Vector *stack, uintptr_t n);
void stack_pop(Vector *stack);
uintptr_t stack_top(Vector *stack);

typedef struct {
	Vector *keys;
	Vector *vals;
} Map;

Map *new_map(void);
void map_put(Map *m, const char *key, void *val);
void *map_get(Map *m, const char *key);

typedef struct {
	const void *key;
	void *val;
} HashItem;

typedef uint32_t (*HashFunc)(const void *key);
// Returns zero if the two keys are equal.
typedef int (*HashKeyCompare)(const void *k1, const void *k2);

typedef struct {
	HashItem *table;
	uint32_t size;	uint32_t occupied;
	HashFunc hash;
	HashKeyCompare compare;
} HashMap;

HashMap *new_hash(HashFunc hash, HashKeyCompare compare);
HashMap *new_string_hash(void);
void hash_put(HashMap *m, const void *key, const void *val);
void *hash_get(HashMap *m, const void *key);
HashItem *hash_iterate(HashMap *m, HashItem *item);

// dri.c

#define DRI_MAX_VOLUME 26

typedef struct {
	int id;      // 1-based
	const uint8_t *data;
	int size;
	uint32_t volume_bits;  // (1 << k) is set if the entry is present in the k-th volume
} DriEntry;

void dri_write(Vector *entries, int volume, FILE *fp);
Vector *dri_read(Vector *entries, const char *path);

// ag00.c

typedef struct {
	Vector *verbs;
	Vector *objs;
	int uk1;
	int uk2;
} AG00;

AG00 *ag00_read(const char *path);
void ag00_write(AG00 *ag00, const char *path);

// game_id.c

typedef enum {
	SYSTEM1 = 1,
	SYSTEM2,
	SYSTEM3,
} SysVer;

typedef enum {
	UNKNOWN_GAME,

	SYSTEM1_GENERIC,
	BUNKASAI,
	CRESCENT,
	DPS,
	DPS_SG_FAHREN,
	DPS_SG_KATEI,
	DPS_SG_NOBUNAGA,
	DPS_SG2_ANTIQUE,
	DPS_SG2_IKENAI,
	DPS_SG2_AKAI,
	DPS_SG3_RABBIT,
	DPS_SG3_SHINKON,
	DPS_SG3_SOTSUGYOU,
	FUKEI,
	INTRUDER,
	TENGU,
	TOUSHIN,
	TOUSHIN_HINT,
	LITTLE_VAMPIRE,
	LITTLE_VAMPIRE_ENG,
	YAKATA,
	GAKUEN,
	GAKUEN_ENG,

	SYSTEM2_GENERIC,
	AYUMI_FD,
	AYUMI_HINT,
	AYUMI_PROTO,
	DALK_HINT,
	DRSTOP,
	PROG_FD,
	RANCE3_HINT,
	SDPS_MARIA,
	SDPS_TONO,
	SDPS_KAIZOKU,
	YAKATA2,
	RANCE4_OPT,

	SYSTEM3_GENERIC,
	AMBIVALENZ_FD,
	AMBIVALENZ_CD,
	DPS_ALL,
	FUNNYBEE_CD,
	FUNNYBEE_FD,
	ONLYYOU,
	ONLYYOU_DEMO,
	PROG_CD,
	PROG_OMAKE,
	RANCE41,
	RANCE41_ENG,
	RANCE42,
	RANCE42_ENG,
	AYUMI_CD,
	AYUMI_LIVE_256,
	AYUMI_LIVE_FULL,
	YAKATA3_CD,
	YAKATA3_FD,
	HASHIRIONNA2,
	TOUSHIN2_GD,
	TOUSHIN2_SP,
	OTOME,
	NINGYO,
	MUGEN,
} GameId;

static inline SysVer get_sysver(GameId id) {
	return id < SYSTEM2_GENERIC ? SYSTEM1 : id < SYSTEM3_GENERIC ? SYSTEM2 : SYSTEM3;
}

GameId game_id_from_name(const char *name);
const char *game_id_to_name(GameId id);
uint32_t calc_crc32(const char* fname);
GameId detect_game_id(uint32_t adisk_crc, uint32_t bdisk_crc);

// opcodes

enum {
	OP_MUL = 0x77,
	OP_DIV,
	OP_ADD,
	OP_SUB,
	OP_EQ,
	OP_LT,
	OP_GT,
	OP_NE,
	OP_END, // End of expression
};

enum {
	// Pseudo commands
	COMMAND_IF = 0x80,
	COMMAND_CONST = 0x81,
	COMMAND_PRAGMA = 0x82,
};
