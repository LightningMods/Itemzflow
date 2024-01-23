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
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <fcntl.h>
#include "ini.h"

std::unordered_map<std::string, std::string> dmpstropts;
static bool lang_is_dumper_initialized = false;
extern "C" int sceSystemServiceParamGetInt(int, int32_t *);
//
// https://github.com/Al-Azif/ps4-payload-guest/blob/main/src/Language.cpp
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

std::string getDumperLangSTR(Lang_ST str)
{
    if (lang_is_dumper_initialized)
    {
        if (dmpstropts[lang_key[str]].empty() || str > LANG_NUM_OF_STRINGS)
            return "String NOT found";
        else
            return dmpstropts[lang_key[str]];
    }
    else
        return "ERROR: Lang has not loaded";
}

static int load_lang_dumper_ini(void* user, const char* section, const char* name,
   const char* value)
{
    dmpstropts.insert(std::pair<std::string, std::string>(name, value));
    //log_info("Loaded %s: %s", name, value);

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
    if (dmpstropts.empty()) {
       int error = ini_parse("/user/app/ITEM00001/lang.ini", load_lang_dumper_ini, NULL);
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
    std::string dst = "/mnt/sandbox/pfsmnt/ITEM00001-app0/langs/" + std::to_string(LangCode) + "/lang.ini";
    if (!lang_is_dumper_initialized) {
        //dont leak or realloc on fail
        if(!std::filesystem::exists((dst.c_str()))){
            log_error("Lang file not found! %s", dst.c_str());
            return false;
        }
           
        int error = ini_parse(dst.c_str(), load_lang_dumper_ini, nullptr);
        if (error < 0)
        {
            log_error("Bad config file (first error on line %d)!", error);
            return false;
        }
        else
            lang_is_dumper_initialized = true;
    }

	return true;
}