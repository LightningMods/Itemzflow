#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include "lang.h"
#include "log.h"
#include <fcntl.h>
#include "ini.h"

static DumperStrings* args;
static bool lang_is_dumper_initialized = false;
//
//https://github.com/Al-Azif/ps4-payload-guest/blob/main/src/Language.cpp
//
int sceSystemServiceParamGetInt(int i, int32_t *lang);
int32_t DumperGetLang() {
    int32_t lang;

    if (sceSystemServiceParamGetInt(1, &lang) != 0) {
        log_error("Unable to qet language code");
        return 0x1;
    }

    return lang;
}

char* getDumperLangSTR(enum Lang_ST str)
{

    if (lang_is_dumper_initialized) {
        if (str > LANG_NUM_OF_STRINGS) {
            return "ERROR";
        }
        else
        {
            if (args->strings[str] != NULL)
                return args->strings[str];
            else
                return "ERROR";
        }
    }
    else
        return "ERROR: Lang has not loaded";
}


static int load_lang_dumper_ini(void* user, const char* section, const char* name,
   const char* value)
{
 
    DumperStrings* set = (DumperStrings*)user;


#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("DUMPER", "DUMP_INFO")) {
        snprintf(set->strings[DUMP_INFO], 255, "%s", value);
        log_debug("%s ", set->strings[DUMP_INFO]);
    }
    else if (MATCH("DUMPER", "APP_NAME")) {
        snprintf(set->strings[APP_NAME], 255, "%s", value);
        log_debug("%s ", set->strings[APP_NAME]);
    }
    else if (MATCH("DUMPER", "TITLE_ID")) {
        snprintf(set->strings[TITLE_ID], 255, "%s", value);
        log_debug("%s ", set->strings[TITLE_ID]);
    }
    else if (MATCH("DUMPER", "DUMPER_SC0")) {
        snprintf(set->strings[DUMPER_SC0], 255, "%s", value);
    }
    else if (MATCH("DUMPER", "DUMPING_TROPHIES")) {
    snprintf(set->strings[DUMPING_TROPHIES], 255, "%s", value);
    }
    else if (MATCH("DUMPER", "CREATING_GP4")) {
    snprintf(set->strings[CREATING_GP4], 255, "%s", value);
    }
    if (MATCH("DUMPER", "DEC_BIN")) {
        snprintf(set->strings[DEC_BIN], 255, "%s", value);//
    }
    else if (MATCH("DUMPER", "DEL_SEM")) {
        snprintf(set->strings[DEL_SEM], 255, "%s", value);//
    }
    else if (MATCH("DUMPER", "EXT_PATCH")) {
        snprintf(set->strings[EXT_PATCH], 255, "%s", value);
    }
    if (MATCH("DUMPER", "EXT_GAME_FILES")) {
        snprintf(set->strings[EXT_GAME_FILES], 255, "%s", value);
    }
    else if (MATCH("DUMPER", "PROCESSING")) {
        snprintf(set->strings[PROCESSING], 255, "%s", value);
    }


    return 1;

}

bool exists(const char* path)
{
    int dfd = open(path, 0, 0); // try to open dir
    if (dfd < 0) {
        log_info("path %s, errno %s", path, strerror(errno));
        return false;
    }
    else
        close(dfd);


    return true;
}

bool load_dumper_embdded_eng()
{
    //mem should already be allocated as this is the last opt
    if (args != NULL) {
       int error = ini_parse("/user/app/ITEM00001/lang.ini", load_lang_dumper_ini, args);
       if (error) {
           log_error("Bad config file (first error on line %d)!", error);
           return false;
       }
       else 
           lang_is_dumper_initialized = true;
       
    }
    else
        return false;

    return true;
}

bool LoadDumperLangs(int LangCode)
{
    if (!lang_is_dumper_initialized) {
        //dont leak or realloc on fail
        if (args == NULL)
            args = (DumperStrings*)malloc(sizeof(DumperStrings));
        if (args != NULL) {
            for (int i = 0; i < LANG_NUM_OF_STRINGS; i++) {
                args->strings[i] = calloc(256, sizeof(char));
                if (args->strings[i] == NULL) {
                    log_info( "Failed to load Language(s)... Failed to allocate args->strings[%i]", i);
                    return false;
                }
            }

            char tmp[100];
            snprintf(tmp, 99, LANG_DIR, LangCode);

            int error = ini_parse(tmp, load_lang_dumper_ini, args);
            if (error) {
                log_error("Bad config file (first error on line %d)!", error);
                return false;
            }
            else
                lang_is_dumper_initialized = true;
        }
        else
            return false;
    }

	return true;
}