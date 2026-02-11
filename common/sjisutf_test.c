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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_msx(void) {
	const char *msg_utf8 = "あいうえおかきくけこがぎぐげごぱぴぷぺぽヴＡＢＣ";
	int len;
	char *msx = utf2msx_msg(msg_utf8, &len);
	char *sjis = msx2sjis_msg(msx, len);
	char *utf8_back = sjis2utf(sjis);
	if (strcmp(msg_utf8, utf8_back) != 0) {
		printf("[FAIL] msx2sjis_msg/utf2msx_msg roundtrip failed\n");
		printf("  Expected: %s\n", msg_utf8);
		printf("  Got:      %s\n", utf8_back);
		exit(1);
	}
	free(msx);
	free(sjis);
	free(utf8_back);

	const char *data_utf8 = "あいうえおＡＢＣ";
	char *msx_data = utf2msx_data(data_utf8);
	char *sjis_data = msx2sjis_data(msx_data);
	char *utf8_data_back = sjis2utf(sjis_data);
	if (strcmp(data_utf8, utf8_data_back) != 0) {
		printf("[FAIL] msx2sjis_data/utf2msx_data roundtrip failed\n");
		printf("  Expected: %s\n", data_utf8);
		printf("  Got:      %s\n", utf8_data_back);
		exit(1);
	}
	free(msx_data);
	free(sjis_data);
	free(utf8_data_back);

	// Test dakuten/handakuten specifically
	// 'が' is 'か' (0x96 in msg table) + dakuten (0xDE)
	char *msx_ga = utf2msx_msg("が", &len);
	if (len != 2 || (uint8_t)msx_ga[0] != 0x96 || (uint8_t)msx_ga[1] != 0xde) {
		printf("[FAIL] utf2msx_msg('が') failed: len=%d, bytes=%02x %02x\n", len, (uint8_t)msx_ga[0], (uint8_t)msx_ga[1]);
		exit(1);
	}
	char *sjis_ga = msx2sjis_msg(msx_ga, len);
	// SJIS for 'が' is 0x82AA.
	if ((uint8_t)sjis_ga[0] != 0x82 || (uint8_t)sjis_ga[1] != 0xaa) {
		printf("[FAIL] msx2sjis_msg('が') failed: bytes=%02x %02x\n", (uint8_t)sjis_ga[0], (uint8_t)sjis_ga[1]);
		exit(1);
	}
	free(msx_ga);
	free(sjis_ga);

	if (!is_msx_message_char(0x91)) { // 'あ'
		printf("[FAIL] is_msx_message_char(0x91) should be true\n");
		exit(1);
	}
	if (is_msx_message_char(0x20)) { // Invalid (ACT command area)
		printf("[FAIL] is_msx_message_char(0x20) should be false\n");
		exit(1);
	}
}

static void test_compaction(void) {
	for (int c = 0; c < 256; c++) {
		if (is_compacted_sjis(c)) {
			uint16_t full = expand_sjis(c);
			if (!full) {
				printf("[FAIL] expand_sjis(0x%02x) unexpectedly returned zero\n", c);
				exit(1);
			}
			uint8_t half = compact_sjis(full >> 8, full & 0xff);
			if (c != half) {
				printf("[FAIL] compact_sjis(0x%04x): expected 0x%02x, got 0x%02x\n", full, c, half);
				exit(1);
			}
		} else {
			uint16_t full = expand_sjis(c);
			if (full) {
				printf("[FAIL] expand_sjis(0x%02x) unexpectedly returned non-zero value 0x%04x\n", c, full);
				exit(1);
			}
		}
	}
	for (int c1 = 0x81; c1 <= 0x82; c1++) {
		for (int c2 = 0x40; c2 <= 0xff; c2++) {
			uint8_t half = compact_sjis(c1, c2);
			if (!half)
				continue;
			uint16_t full = expand_sjis(half);
			if (full != (c1 << 8 | c2)) {
				printf("[FAIL] compact_sjis/expand_sjis: 0x%04x -> 0x%02x -> 0x%04x\n", c1 << 8 | c2, half, full);
			}
		}
	}
}

void sjisutf_test(void) {
	test_compaction();
	test_msx();
}
