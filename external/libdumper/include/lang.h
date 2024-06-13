#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <fcntl.h>

enum Lang_ST {
	DUMP_INFO,
	APP_NAME,
	TITLE_ID,
	DUMPER_SC0,
	DUMPING_TROPHIES,
	CREATING_GP4,
	DEC_BIN,
	DEL_SEM,
	EXT_PATCH,
	EXT_GAME_FILES,
	PROCESSING,
	EXT_ADDON,
	DLC_NAME,
	ASK_FOR_GP4,
   //DO NOT DELETE THIS, ALWAYS KEEP AT THE BOTTOM
   LANG_NUM_OF_STRINGS
};

static const char *lang_key[LANG_NUM_OF_STRINGS] = {
	"DUMP_INFO",
	"APP_NAME",
	"TITLE_ID",
	"DUMPER_SC0",
	"DUMPING_TROPHIES",
	"CREATING_GP4",
	"DEC_BIN",
	"DEL_SEM",
	"EXT_PATCH",
	"EXT_GAME_FILES",
	"PROCESSING",
	"EXT_ADDON",
	"DLC_NAME",
	"ASK_FOR_GP4",
};

typedef struct
{
	std::string strings[LANG_NUM_OF_STRINGS];
} LangStrings;

bool LoadDumperLangs(int LangCode);
std::string getDumperLangSTR(Lang_ST str);
int32_t DumperGetLang();
bool load_dumper_embdded_eng();