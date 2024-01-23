
#include "lang.h"
#include "ini.h"
#include "Header.h"

LangStrings* stropts;
bool lang_is_initialized = false;
int32_t PS4GetLang() {
    int32_t lang;
    if (sceSystemServiceParamGetInt(1, &lang) != 0) {
        logshit("Unable to qet language code");
        return 0x1;
    }
    return lang;
}

char* getLangSTR(enum Lang_STR str)
{

    if (lang_is_initialized) {
        if (str > LANG_NUM_OF_STRINGS) {
            return stropts->strings[STR_NOT_FOUND];
        }
        else
        {
            if (stropts->strings[str])
                return stropts->strings[str];
            else
                return stropts->strings[STR_NOT_FOUND];
        }
    }
    else
        return "ERROR: Lang has not loaded";
}


static int load_lang_ini(void* user, const char* section, const char* name,
    const char* value)
{
    LangStrings* set = (LangStrings*)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("LOADER", "REINSTALL_PKG")) {
        snprintf(set->strings[REINSTALL_PKG], 255, "%s", value);
    }
    else if (MATCH("LOADER", "LOADER_ERROR")) {
        snprintf(set->strings[LOADER_ERROR], 255, "%s", value);
    }
    else if (MATCH("LOADER", "MORE_INFO")) {
        snprintf(set->strings[MORE_INFO], 255, "%s", value);
    }
    else if (MATCH("LOADER", "DOWNLOADING_UPDATE")) {
        snprintf(set->strings[DOWNLOADING_UPDATE], 255, "%s", value);
    }
    else if (MATCH("LOADER", "INI_FAIL")) {
        snprintf(set->strings[INI_FAIL], 255, "%s", value);
    }
    else if (MATCH("LOADER", "UPDATE_REQ")) {
        snprintf(set->strings[UPDATE_REQ], 255, "%s", value);
    }
    else if (MATCH("LOADER", "UPDATE_APPLIED")) {
        snprintf(set->strings[UPDATE_APPLIED], 255, "%s", value);
    }
    else if (MATCH("LOADER", "OPT_UPDATE")) {
        snprintf(set->strings[OPT_UPDATE], 255, "%s", value);
    }
    else if (MATCH("LOADER", "LOADER_FATAL")) {
        snprintf(set->strings[LOADER_FATAL], 255, "%s", value);
    }
    else if (MATCH("LOADER", "FATAL_JB")) {
        snprintf(set->strings[FATAL_JB], 255, "%s", value);
    }
    else if (MATCH("LOADER", "FATAL_REJAIL")) {
        snprintf(set->strings[FATAL_REJAIL], 255, "%s", value);
    }
    else if (MATCH("LOADER", "EXT_NOT_SUPPORTED")) {
        snprintf(set->strings[EXT_NOT_SUPPORTED], 255, "%s", value);
    }
    else if (MATCH("LOADER", "SWU_ERROR")) {
        snprintf(set->strings[SWU_ERROR], 255, "%s", value);
    }
    else if (MATCH("LOADER", "RSA_LOAD")) {
        snprintf(set->strings[RSA_LOAD], 255, "%s", value);//
    }
    else if (MATCH("LOADER", "RSA_FAILED")) {
        snprintf(set->strings[RSA_FAILED], 255, "%s", value);//
    }
    else if (MATCH("LOADER", "SECURE_FAIL")) {
        snprintf(set->strings[SECURE_FAIL], 255, "%s", value);
    }
    else if (MATCH("LOADER", "STR_NOT_FOUND")) {
        snprintf(set->strings[STR_NOT_FOUND], 255, "%s", value);
    }
    else if (MATCH("LOADER", "PING_FAILED")) {
        snprintf(set->strings[PING_FAILED], 255, "%s", value);
    }

    return 1;

}

bool LoadLangs(int LangCode)
{

    stropts = (LangStrings*)malloc(sizeof(LangStrings));
    if (stropts != NULL) {
        for (int i = 0; i < LANG_NUM_OF_STRINGS; i++) {
            stropts->strings[i] = calloc(256, sizeof(char));

            if (stropts->strings[i] == NULL) {
                msgok("Failed to load Language(s)... Failed to allocate stropts->strings[%i]", i);
                return false;
            }
        }

        char tmp[100];
        snprintf(tmp, 99, LANG_DIR, LangCode);

        int error = ini_parse(tmp, load_lang_ini, stropts);
        if (error) {
            logshit("Bad config file (first error on line %d)!", error);
            return false;
        }
        else
            lang_is_initialized = true;
    }
    else{
        msgok("LoadLangs failed");
        return false;
    }

    return true;
}