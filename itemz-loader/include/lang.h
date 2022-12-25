#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
enum Lang_STR {
	LOADER_FATAL,
	FATAL_JB,
	FATAL_REJAIL,
	EXT_NOT_SUPPORTED,
	PING_FAILED,
	SWU_ERROR,
	RSA_LOAD,
	RSA_FAILED,
	SECURE_FAIL,
	REINSTALL_PKG,
	LOADER_ERROR,
	MORE_INFO,
	DOWNLOADING_UPDATE,
	INI_FAIL,
	UPDATE_REQ,
	UPDATE_APPLIED,
	OPT_UPDATE,
	STR_NOT_FOUND,
	//DO NOT DELETE THIS, ALWAYS KEEP AT THE BOTTOM
	LANG_NUM_OF_STRINGS
};

typedef struct
{
	char* strings[LANG_NUM_OF_STRINGS];
} LangStrings;


#define LANG_DIR "/app0/langs/%i/lang.ini"

bool LoadLangs(int LangCode);
char* getLangSTR(enum Lang_STR str);
int32_t PS4GetLang();