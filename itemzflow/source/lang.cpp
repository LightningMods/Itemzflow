#include "defines.h"
#include "lang.h"
#include <unordered_map>

bool lang_is_initialized = false;
extern int numb_of_settings;
std::unordered_map<std::string, std::string> stropts;
std::vector<std::string> gm_p_text(20);
std::vector<std::string> ITEMZ_SETTING_TEXT(NUMBER_OF_SETTINGS + 10);
using namespace LANG_STR;
//
// https://github.com/Al-Azif/ps4-payload-guest/blob/main/src/Language.cpp
//
std::string Language_GetName(int m_Code)
{
    std::string s_Name;

    switch (m_Code)
    {
    case 0:
        s_Name = "Japanese";
        break;
    case 1:
        s_Name = "English (United States)";
        break;
    case 2:
        s_Name = "French (France)";
        break;
    case 3:
        s_Name = "Spanish (Spain)";
        break;
    case 4:
        s_Name = "German";
        break;
    case 5:
        s_Name = "Italian";
        break;
    case 6:
        s_Name = "Dutch";
        break;
    case 7:
        s_Name = "Portuguese (Portugal)";
        break;
    case 8:
        s_Name = "Russian";
        break;
    case 9:
        s_Name = "Korean";
        break;
    case 10:
        s_Name = "Chinese (Traditional)";
        break;
    case 11:
        s_Name = "Chinese (Simplified)";
        break;
    case 12:
        s_Name = "Finnish";
        break;
    case 13:
        s_Name = "Swedish";
        break;
    case 14:
        s_Name = "Danish";
        break;
    case 15:
        s_Name = "Norwegian";
        break;
    case 16:
        s_Name = "Polish";
        break;
    case 17:
        s_Name = "Portuguese (Brazil)";
        break;
    case 18:
        s_Name = "English (United Kingdom)";
        break;
    case 19:
        s_Name = "Turkish";
        break;
    case 20:
        s_Name = "Spanish (Latin America)";
        break;
    case 21:
        s_Name = "Arabic";
        break;
    case 22:
        s_Name = "French (Canada)";
        break;
    case 23:
        s_Name = "Czech";
        break;
    case 24:
        s_Name = "Hungarian";
        break;
    case 25:
        s_Name = "Greek";
        break;
    case 26:
        s_Name = "Romanian";
        break;
    case 27:
        s_Name = "Thai";
        break;
    case 28:
        s_Name = "Vietnamese";
        break;
    case 29:
        s_Name = "Indonesian";
        break;
    default:
        s_Name = "UNKNOWN";
        break;
    }

    return s_Name;
}

int32_t PS4GetLang()
{
    int32_t lang = 1;
#if defined(__ORBIS__)
    if (sceSystemServiceParamGetInt(1, &lang) != 0)
    {
        log_error("Unable to qet language code");
        return 0x1;
    }
#endif
    return lang;
}

std::string getLangSTR(LANG_STR::LANG_STRINGS str)
{
    if (lang_is_initialized)
    {
        if (str > LANG_NUM_OF_STRINGS || stropts[lang_key[str]].empty() )
        {
            return stropts[lang_key[STR_NOT_FOUND]];
        }
        else
            return stropts[lang_key[str]];
    }
    else
        return "ERROR: Lang has not loaded";
}

static int load_lang_ini(void *, const char *, const char *name,
                         const char *value, int)
{

    stropts.insert(std::pair<std::string, std::string>(name, value));

    return true;
}

void fill_menu_text()
{

    if (numb_of_settings > NUMBER_OF_SETTINGS){
        log_info("In Advanced menu, skipping..");
        return;
    }
    ITEMZ_SETTING_TEXT[0] = fmt::format("{0:.20}", getLangSTR(LANG_STR::SETTINGS_1));
    ITEMZ_SETTING_TEXT[1] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_2));
    ITEMZ_SETTING_TEXT[2] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_3));
    ITEMZ_SETTING_TEXT[3] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_4));
    ITEMZ_SETTING_TEXT[4] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_5));
    ITEMZ_SETTING_TEXT[5] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_6));
    ITEMZ_SETTING_TEXT[6] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_7));
    ITEMZ_SETTING_TEXT[7] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_8));
    ITEMZ_SETTING_TEXT[8] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_9));
    ITEMZ_SETTING_TEXT[9] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_10));
    ITEMZ_SETTING_TEXT[10] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_11));

    ITEMZ_SETTING_TEXT[11] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_12));

    ITEMZ_SETTING_TEXT[12] = fmt::format("{0:.30}", getLangSTR(LANG_STR::EXTRA_SETTING_1));
    ITEMZ_SETTING_TEXT[13] = fmt::format("{0:.30}", getLangSTR(LANG_STR::EXTRA_SETTING_2));
    ITEMZ_SETTING_TEXT[14] = fmt::format("{0:.30}", getLangSTR(LANG_STR::EXTRA_SETTING_3));
    ITEMZ_SETTING_TEXT[15] = fmt::format("{0:.30}", getLangSTR(LANG_STR::EXTRA_SETTING_4));
    ITEMZ_SETTING_TEXT[16] = fmt::format("{0:.30}", getLangSTR(LANG_STR::EXTRA_SETTING_5));
    ITEMZ_SETTING_TEXT[17] = fmt::format("{0:.30}", getLangSTR(LANG_STR::EXTRA_SETTING_6));

    gm_p_text[0] = fmt::format("{0:.20}", getLangSTR(LANG_STR::LAUNCH_GAME));
    gm_p_text[1] = fmt::format("{0:.20}", getLangSTR(LANG_STR::DUMP_1));
    gm_p_text[2] = fmt::format("{0:.20}", getLangSTR(LANG_STR::UNINSTALL));
    gm_p_text[3] = fmt::format("{0:.20}", getLangSTR(LANG_STR::RETAIL_UPDATES));
    gm_p_text[4] = fmt::format("{0:.20}", getLangSTR(LANG_STR::TRAINERS));
    gm_p_text[5] = fmt::format("{0:.20}", getLangSTR(LANG_STR::HIDE_APP_OPT));
    gm_p_text[6] = fmt::format("{0:.20}", getLangSTR(LANG_STR::CHANGE_ICON));
    gm_p_text[7] = fmt::format("{0:.20}", getLangSTR(LANG_STR::CHANGE_APP_NAME));
    gm_p_text[8] = fmt::format("{0:.20}", getLangSTR(LANG_STR::MOVE_APP_OPT));
    gm_p_text[9] = fmt::format("{0:.20}", getLangSTR(LANG_STR::RESTORE_APP_OPT));
}
// OVERWRITE_SAVE
extern uint8_t lang_ini[];
extern int32_t lang_ini_sz;

#if defined(__ORBIS__)
bool load_embdded_eng()
{
    // mem should already be allocated as this is the last opt
    int fd = -1;
    if (!if_exists("/user/app/ITEM00001/lang.ini"))
    {
        if ((fd = open("/user/app/ITEM00001/lang.ini", O_WRONLY | O_CREAT | O_TRUNC, 0777)) > 0 && fd != -1)
        {
            write(fd, lang_ini, lang_ini_sz);
            close(fd);
        }
        else
            return false;
    }

    int error = ini_parse("/user/app/ITEM00001/lang.ini", load_lang_ini, nullptr);
    if (error)
    {
        log_error("Bad config file (first error on line %d)!\n", error);
        return false;
    }
    else
        lang_is_initialized = true;

    fill_menu_text();

    return true;
}
#else

bool load_embdded_eng()
{
    // mem should already be allocated as this is the last opt
    // dont leak or realloc on fail

    int fd = -1;

    int error = ini_parse("lang/lang.ini", load_lang_ini, nullptr);
    if (error)
    {
        log_error("Bad config file (first error on line %d)!\n", error);
        return false;
    }
    else
        lang_is_initialized = true;

    fill_menu_text();
    return true;
}

#endif

bool LoadLangs(int LangCode)
{

    std::string dst = fmt::format("{0:}/{1:d}/lang.ini", LANG_DIR, LangCode);
#ifndef TEST_INI_LANGS
    if (!lang_is_initialized)
#else
    if (1)
#endif
    {
        if(!if_exists(dst.c_str())){
            log_error("Lang file not found! %s", dst.c_str());
            return false;
        }
           
        int error = ini_parse(dst.c_str(), load_lang_ini, nullptr);
        if (error < 0)
        {
            log_error("Bad config file (first error on line %d)!", error);
            return false;
        }
        else
            lang_is_initialized = true;
    }

    fill_menu_text();
    return true;
}