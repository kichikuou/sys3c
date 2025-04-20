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
#include <string.h>

#define ADISK_BUNKASAI		0xc80f99b8	// あぶない文化祭前夜
#define ADISK_CRESCENT		0x42351f2c	// クレセントムーンがぁる
#define ADISK_RANCE		0x2fffbd60	// Rance -光をもとめて-
#define ADISK_RANCE2		0x28f8298f	// Rance 2
#define ADISK_RANCE2_HINT	0x2a85e5fa	// Rance 2 ヒントディスク (GDISK.DAT)
#define ADISK_DPS		0x69ea4865	// D.P.S. - Dream Program System
#define ADISK_DPS_SG		0xab4cda48	// D.P.S. SG / D.P.S. SG set2
#define BDISK_DPS_SG_FAHREN	0xe405d57c	//	D.P.S. SG - Fahren Fliegen
#define BDISK_DPS_SG_KATEI	0x23e67d18	//	D.P.S. SG - 家庭教師はステキなお仕事
#define BDISK_DPS_SG_NOBUNAGA	0x2ec116f2	//	D.P.S. SG - 信長の淫謀
#define BDISK_DPS_SG2_ANTIQUE	0x41fe8b3d	//	D.P.S. SG set2 - ANTIQUE HOUSE
#define BDISK_DPS_SG2_IKENAI	0x6b562c09	//	D.P.S. SG set2 - いけない内科検診再び
#define BDISK_DPS_SG2_AKAI	0x1098e78c	//	D.P.S. SG set2 - 朱い夜
#define ADISK_DPS_SG3		0xb77ae133	// D.P.S. SG SG set3
#define BDISK_DPS_SG3_RABBIT	0xa3228b6c	//	D.P.S. SG set3 - Rabbit P4P
#define BDISK_DPS_SG3_SHINKON	0x09b4448a	//	D.P.S. SG set3 - しんこんさんものがたり
#define BDISK_DPS_SG3_SOTSUGYOU	0xbc4525d8	//	D.P.S. SG set3 - 卒業
#define ADISK_FUKEI		0x026de326	// 婦警さんＶＸ
#define ADISK_INTRUDER		0xa7520fb2	// Intruder -桜屋敷の探索-
#define ADISK_TENGU		0xc942ff58	// あぶないてんぐ伝説
#define ADISK_TOUSHIN		0x62327908	// 闘神都市
#define ADISK_TOUSHIN_HINT	0xac337537	// 闘神都市 ヒントディスク
#define ADISK_VAMPIRE		0x957bcfbf	// Little Vampire
#define ADISK_VAMPIRE_ENG	0x61985a7f	// Little Vampire (English) Patch 1.5
#define ADISK_YAKATA		0x8cef6fa6	// ALICEの館
#define ADISK_GAKUEN		0xe4d6ec66	// 学園戦記 (unofficial system1 port) 1.0JP
#define ADISK_GAKUEN_ENG	0x6ba8c102	// Gakuen Senki (English) 1.0
#define ADISK_AYUMI_FD		0x4e2fed2a	// あゆみちゃん物語 (FD)
#define ADISK_AYUMI_HINT	0xf6bd963a	// あゆみちゃん物語 ヒントディスク
#define ADISK_AYUMI_PROTO	0x4e2f5678	// あゆみちゃん物語 PROTO
#define ADISK_DALK		0x77227088	// DALK
#define ADISK_DALK_HINT		0x4793b843	// DALK ヒントディスク
#define ADISK_DRSTOP		0x73fa86c4	// Dr. STOP!
#define ADISK_PROSTUDENTG_FD	0x5ffbfee7	// Prostudent -G- (FD)
#define ADISK_RANCE3		0x47a399a1	// Rance 3
#define ADISK_RANCE3_HINT	0x8d5ec610	// Rance3 ヒントディスク
#define ADISK_SDPS		0xc7a20cdf	// Super D.P.S
#define BDISK_SDPS_MARIA	0x80d4eaca	//	Super D.P.S - マリアとカンパン
#define BDISK_SDPS_TONO		0xbb1edff1	//	Super D.P.S - 遠野の森
#define BDISK_SDPS_KAIZOKU	0xf81829e3	//	Super D.P.S - うれしたのし海賊稼業
#define ADISK_YAKATA2		0x2df591ff	// ALICEの館II
#define ADISK_RANCE4		0xebcfaff1	// Rance 4
#define ADISK_RANCE4_OPT	0xbe91c161	// Rance 4 オプションディスク
#define ADISK_AMBIVALENZ_FD	0xa6b48dfe	// AmbivalenZ (FD)
#define ADISK_AMBIVALENZ_CD	0x4b10db69	// AmbivalenZ (CD)
#define ADISK_DPSALL		0xd48b4ec6	// DPS全部
#define ADISK_FUNNYBEE_CD	0xe14e3971	// Funny Bee (CD)
#define ADISK_FUNNYBEE_PATCH	0xe14e3971	// Funny Bee (CD + Patch)
#define ADISK_FUNNYBEE_FD	0x731267fa	// Funny Bee (FD)
#define ADISK_ONLYYOU		0x832aeb97	// Only You
#define ADISK_ONLYYOU_DEMO	0xc1d13e44	// Only You (DEMO)
#define ADISK_PROSTUDENTG_CD	0xfb0e4a63	// Prostudent -G- (CD)
#define ADISK_PROG_OMAKE	0x8ba18bff	// Prostudent G おまけ (CRC32 of AGAME.DAT)
#define ADISK_RANCE41		0xa43fb4b6	// Rance 4.1
#define ADISK_RANCE41_ENG	0x811f4ff3	// Rance 4.1 (English) 1.5 Beta
#define ADISK_RANCE42		0x04d24d1e	// Rance 4.2
#define ADISK_RANCE42_ENG	0xa97cc370	// Rance 4.2 (English) 1.5 Beta
#define ADISK_AYUMI_CD		0xd2bed9ee	// あゆみちゃん物語 (CD)
#define ADISK_AYUMI_JISSHA_256	0x00d15a2b	// あゆみちゃん物語 実写版
#define ADISK_AYUMI_JISSHA_FULL	0x5f66ff1d	// あゆみちゃん物語 フルカラー実写版
#define ADISK_YAKATA3_CD	0x7f8f5e2a	// アリスの館３ (CD)
#define ADISK_YAKATA3_FD	0x58ebcc99	// アリスの館３ (FD)
#define ADISK_HASHIRIONNA2	0x09f47cbd	// 走り女２ (Rance 4.x ヒントディスク)
#define ADISK_TOUSHIN2_GD	0xb5eba798	// 闘神都市２ グラフィックディスク
#define ADISK_TOUSHIN2_SP	0x2172c7b2	// 闘神都市２ そして、それから…
#define ADISK_OTOMESENKI	0x49a4db15	// 乙女戦記
#define ADISK_NINGYO		0xd491e7ab	// 人魚 -蘿子-
#define ADISK_MUGENHOUYOU	0xbb27d1ba	// 夢幻泡影
#define ADISK_TOUSHIN2		0xe27dd441	// 闘神都市２
#define ADISK_NISE_NAGURI	0xfabe6302	// にせなぐりまくりたわぁ (ADISK.PAT)
#define ADISK_GAKUEN_KING	0xd1bf243b	// 学園KING -日出彦 学校をつくる-

static const struct GameTable {
	const char* name;
	GameId id;
	uint32_t adisk_crc;
	uint32_t bdisk_crc;
} game_table[] = {
	{"system1_generic", SYSTEM1_GENERIC},
	{"bunkasai", BUNKASAI, ADISK_BUNKASAI},
	{"crescent", CRESCENT, ADISK_CRESCENT},
	{"rance", RANCE, ADISK_RANCE},
	{"rance2", RANCE2, ADISK_RANCE2},
	{"rance2_hint", RANCE2_HINT, ADISK_RANCE2_HINT},
	{"dps", DPS, ADISK_DPS},
	{"dps_sg_fahren", DPS_SG_FAHREN, ADISK_DPS_SG, BDISK_DPS_SG_FAHREN},
	{"dps_sg_katei", DPS_SG_KATEI, ADISK_DPS_SG, BDISK_DPS_SG_KATEI},
	{"dps_sg_nobunaga", DPS_SG_NOBUNAGA, ADISK_DPS_SG, BDISK_DPS_SG_NOBUNAGA},
	{"dps_sg2_antique", DPS_SG2_ANTIQUE, ADISK_DPS_SG, BDISK_DPS_SG2_ANTIQUE},
	{"dps_sg2_ikenai", DPS_SG2_IKENAI, ADISK_DPS_SG, BDISK_DPS_SG2_IKENAI},
	{"dps_sg2_akai", DPS_SG2_AKAI, ADISK_DPS_SG, BDISK_DPS_SG2_AKAI},
	{"dps_sg3_rabbit", DPS_SG3_RABBIT, ADISK_DPS_SG3, BDISK_DPS_SG3_RABBIT},
	{"dps_sg3_shinkon", DPS_SG3_SHINKON, ADISK_DPS_SG3, BDISK_DPS_SG3_SHINKON},
	{"dps_sg3_sotsugyou", DPS_SG3_SOTSUGYOU, ADISK_DPS_SG3, BDISK_DPS_SG3_SOTSUGYOU},
	{"fukei", FUKEI, ADISK_FUKEI},
	{"intruder", INTRUDER, ADISK_INTRUDER},
	{"tengu", TENGU, ADISK_TENGU},
	{"toushin", TOUSHIN, ADISK_TOUSHIN},
	{"toushin_hint", TOUSHIN_HINT, ADISK_TOUSHIN_HINT},
	{"little_vampire", LITTLE_VAMPIRE, ADISK_VAMPIRE},
	{"little_vampire_eng", LITTLE_VAMPIRE_ENG, ADISK_VAMPIRE_ENG},
	{"yakata", YAKATA, ADISK_YAKATA},
	{"gakuen", GAKUEN, ADISK_GAKUEN},
	{"gakuen_eng", GAKUEN_ENG, ADISK_GAKUEN_ENG},

	{"system2_generic", SYSTEM2_GENERIC},
	{"ayumi_fd", AYUMI_FD, ADISK_AYUMI_FD},
	{"ayumi_hint", AYUMI_HINT, ADISK_AYUMI_HINT},
	{"ayumi_proto", AYUMI_PROTO, ADISK_AYUMI_PROTO},
	{"dalk", DALK, ADISK_DALK},
	{"dalk_hint", DALK_HINT, ADISK_DALK_HINT},
	{"drstop", DRSTOP, ADISK_DRSTOP},
	{"prog_fd", PROG_FD, ADISK_PROSTUDENTG_FD},
	{"rance3", RANCE3, ADISK_RANCE3},
	{"rance3_hint", RANCE3_HINT, ADISK_RANCE3_HINT},
	{"sdps_maria", SDPS_MARIA, ADISK_SDPS, BDISK_SDPS_MARIA},
	{"sdps_tono", SDPS_TONO, ADISK_SDPS, BDISK_SDPS_TONO},
	{"sdps_kaizoku", SDPS_KAIZOKU, ADISK_SDPS, BDISK_SDPS_KAIZOKU},
	{"yakata2", YAKATA2, ADISK_YAKATA2},
	{"rance4", RANCE4, ADISK_RANCE4},
	{"rance4_opt", RANCE4_OPT, ADISK_RANCE4_OPT},

	{"system3_generic", SYSTEM3_GENERIC},
	{"ambivalenz_fd", AMBIVALENZ_FD, ADISK_AMBIVALENZ_FD},
	{"ambivalenz_cd", AMBIVALENZ_CD, ADISK_AMBIVALENZ_CD},
	{"dps_all", DPS_ALL, ADISK_DPSALL},
	{"funnybee_cd", FUNNYBEE_CD, ADISK_FUNNYBEE_CD},
	{"funnybee_fd", FUNNYBEE_FD, ADISK_FUNNYBEE_FD},
	{"onlyyou", ONLYYOU, ADISK_ONLYYOU},
	{"onlyyou_demo", ONLYYOU_DEMO, ADISK_ONLYYOU_DEMO},
	{"prog_cd", PROG_CD, ADISK_PROSTUDENTG_CD},
	{"prog_omake", PROG_OMAKE, ADISK_PROG_OMAKE},
	{"rance41", RANCE41, ADISK_RANCE41},
	{"rance41_eng", RANCE41_ENG, ADISK_RANCE41_ENG},
	{"rance42", RANCE42, ADISK_RANCE42},
	{"rance42_eng", RANCE42_ENG, ADISK_RANCE42_ENG},
	{"ayumi_cd", AYUMI_CD, ADISK_AYUMI_CD},
	{"ayumi_live_256", AYUMI_LIVE_256, ADISK_AYUMI_JISSHA_256},
	{"ayumi_live_full", AYUMI_LIVE_FULL, ADISK_AYUMI_JISSHA_FULL},
	{"yakata3_cd", YAKATA3_CD, ADISK_YAKATA3_CD},
	{"yakata3_fd", YAKATA3_FD, ADISK_YAKATA3_FD},
	{"hashirionna2", HASHIRIONNA2, ADISK_HASHIRIONNA2},
	{"toushin2_gd", TOUSHIN2_GD, ADISK_TOUSHIN2_GD},
	{"toushin2_sp", TOUSHIN2_SP, ADISK_TOUSHIN2_SP},
	{"otome", OTOME, ADISK_OTOMESENKI},
	{"ningyo", NINGYO, ADISK_NINGYO},
	{"mugen", MUGEN, ADISK_MUGENHOUYOU},

	{"toushin2", TOUSHIN2, ADISK_TOUSHIN2},
	{"nise_naguri", NISE_NAGURI, ADISK_NISE_NAGURI},
	{"gakuen_king", GAKUEN_KING, ADISK_GAKUEN_KING},
	{NULL},
};

GameId game_id_from_name(const char *name) {
	for (const struct GameTable *p = game_table; p->name; p++) {
		if (!strcasecmp(p->name, name))
			return p->id;
	}
	return UNKNOWN_GAME;
}

const char *game_id_to_name(GameId id) {
	for (const struct GameTable *p = game_table; p->name; p++) {
		if (p->id == id)
			return p->name;
	}
	return NULL;
}

uint32_t calc_crc32(const char* fname) {
	static uint32_t table[256];
	if (!table[1]) {
		for (int i = 0; i < 256; i++) {
			uint32_t c = i;
			for(int j = 0; j < 8; j++) {
				if (c & 1) {
					c = (c >> 1) ^ 0xedb88320;
				} else {
					c >>= 1;
				}
			}
			table[i] = c;
		}
	}

	FILE *fp = checked_fopen(fname, "rb");
	uint32_t crc = 0;
	for (int i = 0; i < 256; i++) {
		int d = fgetc(fp);
		uint32_t c = ~crc;
		c = table[(c ^ d) & 0xff] ^ (c >> 8);
		crc = ~c;
	}
	return crc;
}

GameId detect_game_id(uint32_t adisk_crc, uint32_t bdisk_crc) {
	for (const struct GameTable *p = game_table; p->name; p++) {
		if (p->adisk_crc == adisk_crc) {
			if (!p->bdisk_crc || p->bdisk_crc == bdisk_crc) {
				return p->id;
			}
		}
	}
	return UNKNOWN_GAME;
}
