#pragma once
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

   //DO NOT DELETE THIS, ALWAYS KEEP AT THE BOTTOM
   LANG_NUM_OF_STRINGS
};

typedef struct
{
	char* strings[LANG_NUM_OF_STRINGS];
} DumperStrings;


#define LANG_DIR "/mnt/sandbox/pfsmnt/ITEM00001-app0/langs/%i/lang.ini"

bool LoadDumperLangs(int LangCode);
char* getDumperLangSTR(enum Lang_ST str);
int32_t DumperGetLang();
bool load_dumper_embdded_eng();