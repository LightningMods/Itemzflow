#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ps4sdk.h>
#include <stdlib.h>  // malloc, qsort, free
#include <unistd.h>  // close
#include <sys/signal.h>
#include "defines.h"
#include <stdbool.h>
#include <utils.h>
#include <errno.h>
#include <sys/socket.h>
#include <user_mem.h> 
#include "lang.h"
#include <sqlite3.h>
#include <signal.h>
#include <sys/mount.h>
extern Dump_Multi_Sels dump;
extern std::vector<std::string> gm_p_text;
#if defined (__ORBIS__)

#include <ps4sdk.h>
#include <debugnet.h>
#include <orbislink.h>
#include <libkernel.h>
#include <sys/mman.h>
#include <orbisPad.h>
#include <utils.h>
#include <md5.h>
#include <dialog.h>
#include <filesystem>
#else // on linux

#include <stdio.h>
#endif

#include <sys/mount.h>
extern "C" void loadModulesVanilla();

extern OrbisGlobalConf globalConf;
int total_pages;

int s_piglet_module = -1;
int s_shcomp_module = -1;
int JsonErr = 0,
    page;


extern ItemzSettings set,
                   *get;

#define SCE_SYSMODULE_INTERNAL_AUDIOOUT 0x80000001
#define SCE_SYSMODULE_INTERNAL_AUDIOIN 0x80000002

bool is_connected_app = false;

std::string completeVersion;

void generate_build_time(){

    completeVersion = VERSION_MAJOR_INIT;
    completeVersion += ".";
    completeVersion += VERSION_MINOR_INIT;
    completeVersion += "-V-";
    completeVersion += BUILD_YEAR_CH0;
    completeVersion += BUILD_YEAR_CH1;
    completeVersion += BUILD_YEAR_CH2;
    completeVersion += BUILD_YEAR_CH3;
    completeVersion += "-";
    completeVersion += BUILD_MONTH_CH0;
    completeVersion += BUILD_MONTH_CH1;
    completeVersion += "-";
    completeVersion += BUILD_DAY_CH0;
    completeVersion += BUILD_DAY_CH1;
    completeVersion += "T";
    completeVersion += BUILD_HOUR_CH0;
    completeVersion += BUILD_HOUR_CH1;
    completeVersion += ":";
    completeVersion += BUILD_MIN_CH0;
    completeVersion += BUILD_MIN_CH1;
    completeVersion += ":";
    completeVersion += BUILD_SEC_CH0;
    completeVersion += BUILD_SEC_CH1;

    fmt::print("ItemzCore version: {}", completeVersion);
}


/*
    GLES2 scene

    my implementation of an interactive menu, from scratch.
    (shaders
     beware)

    2020-21, masterzorag & LM

    each time, 'l' stand for the current layout we are drawing, on each frame
*/

#include <stdio.h>
#include <string.h>
//#include <stdatomic.h>
#include <time.h>
#include "shaders.h"

#if defined(__ORBIS__)
#include <orbisPad.h>
extern OrbisPadConfig* confPad;
#include <installpkg.h>

#endif

#include <freetype-gl.h>
// freetype-gl pass last composed Text_Length in pixel, we use to align text!
extern float tl;

#include "defines.h"
#include "GLES2_common.h"

layout_t* active_p = NULL; // the one we control

#include "utils.h"

int install_ret = -1, http_ret = -1;

bool install_done = false;

// the Settings
extern ItemzSettings set,
* get;

// related indexes from json.h enum
// for sorting entries
unsigned char cmp_token = 0,
sort_patterns[10] = { 0, 1, 4, 5, 9, 10, 11, 12, 13, 16 };

/* set comparison token */
void set_cmp_token(const int index)
{    //cmp_token = index;
    cmp_token = sort_patterns[index];
}

#include "net.h"
// share from here resolution or other
vec2 resolution;
extern vec2 p1_pos;
vec3 offset;

/* normalized rgba colors */
vec4 sele, // default for selection (blue)
grey, // rect fg
white = (vec4)(1.),
col = (vec4){ .8164, .8164, .8125, 1. }, // text fg
t_sel;

extern double u_t;
extern int    selected_icon;
ivec4  menu_pos = (0),
rela_pos = (0);

extern texture_font_t* main_font, // small
* sub_font, // left panel
* titl_font;
extern vertex_buffer_t* text_buffer[4];
// posix threads args, for downloading
extern dl_arg_t* pt_info;
// available num of jsons
int json_c = 0;
std::vector<item_t> all_apps;

// common title in different panels
GLuint fallback_t = 0;  // fallback texture


// option
extern bool use_reflection,
use_pixelshader;

int layout_fill_item_from_list(layout_t *l, std::vector<std::string> &i_list)
{
    int i, count = 0;
    log_debug("layout_fill_item_from_list size: %d", i_list.size());
    l->item_d = std::vector<item_t>(l->item_c);

    for (i = 0; i < l->item_c; i++) // iterate item_d
    {   // dynallocs each token_d
        l->item_d[i].token_c = 1;
        // we use just the first token_d
        l->item_d[i].name = i_list.at(i);
        if (1)
        {
            //token->len = strlen( i_list[i] );
            count += 1;
            log_debug("%s, %s, %d", __FUNCTION__, l->item_d[i].name.c_str(), count);
        }
    }

    /* update */
    l->item_c = count;      // layout items count
    layout_update_fsize(l); // and the field_size

    return count;
}
// testing new way to detect a view
bool is_Installed_Apps = false;

// no page 0, start from 1
void init_ItemzCore(int w, int h)
{
    resolution = (vec2){ w, h };
    // reset player pos
    p1_pos = (vec2)(0);
    // normalize colors
    sele = (vec4){ 29, 134, 240, 256 } / 256, // blue
        grey = (vec4){ 66,  66,  66, 256 } / 256; // fg

    /* first, scan folder, populate Installed_Apps */
    loadmsg(getLangSTR(LOADING_GAME_LIST));
    // build an array of indexes for each one entry there
    mkdir("/user/app/", 0777);
    // avoid non existent path issues
    mkdir("/user/data/GoldHEN/", 0777);
    mkdir("/user/data/GoldHEN/patches/", 0777);
    mkdir("/user/data/GoldHEN/patches/settings/", 0777);
    mkdir("/user/data/GoldHEN/patches/json/", 0777);
    log_info("Searching for Apps");

    index_items_from_dir(all_apps, "/user/app", "/mnt/ext0/user/app", NO_CATEGORY_FILTER);

    // setup one for all missing, the fallback icon0
    if (!fallback_t)
    {
        if (if_exists("/user/appmeta/ITEM00001/icon0.png"))
            fallback_t = load_png_asset_into_texture("/user/appmeta/ITEM00001/icon0.png");
        else
            fallback_t = load_png_asset_into_texture("/user/appmeta/external/ITEM00001/icon0.png");
    }

    menu_pos.z = ON_ITEMzFLOW;

    /* UI: menu */
    // init shaders for textures
    on_GLES2_Init_icons(w, h);
    // init shaders for lines and rects
    ORBIS_RenderFillRects_init(w, h);
    // init ttf fonts
    GLES2_fonts_from_ttf(get->setting_strings[FNT_PATH].c_str());
    // init ani_notify()
    GLES2_ani_init(w, h);
    // fragments shaders, effects etc
    pixelshader_init(w, h);
    // badges
    // GLES2_Init_badge();
    // coverflow alike, two parts for now
    InitScene_4(w, h);
    InitScene_5(w, h);

    if(get->sort_by != NA_SORT){
       log_info("Sorting by %d", get->sort_by);
       refresh_apps_for_cf(get->sort_by, get->sort_cat);
    }

}


void GLES2_UpdateVboForLayout(layout_t* l)
{
    if (l)
    {   // skip until we ask to refresh VBO
        if (l->vbo_s < ASK_REFRESH) return;

        if (l->vbo) vertex_buffer_delete(l->vbo), l->vbo = NULL;

        l->vbo_s = EMPTY;
    }
}

void O_action_dispatch(void)
{
    refresh_atlas();
}


bool init_daemon_services(bool redirect)
{
    char IPC_BUFFER[100];

    get->ItemzDaemon_AppId = sceLncUtilGetAppId((char*)"ITEM00002");
    //Launch Daemon with silent    
    if ((get->ItemzDaemon_AppId & ~0xFFFFFF) != 0x60000000) {
        log_error("Daemon not launched: error %x", get->ItemzDaemon_AppId);
        return false;
    }
    else
        log_info("Found Daemon AppId: %x", get->ItemzDaemon_AppId);

    loadmsg(getLangSTR(WAITING_FOR_DAEMON));
    int error = INVALID, wait = INVALID;
    // Wait for the Daemon to respond
    do {

        error = IPCSendCommand(CONNECTION_TEST, (uint8_t*)&IPC_BUFFER[0]);
        log_info("[ItemzDaemon] Status: %s", error == INVALID ? "Failed to Connect" : "Success");
        if (error == NO_ERROR) {
            sceMsgDialogTerminate();
            log_debug(" Took the Daemon %i extra commands attempts to respond", wait);
            is_connected_app = true;
        }

        if (wait >= 60)
            break;

        sleep(1);
        wait++;

     //File Flag the Daemon creates when initialization is complete
    // and the Daemon IPC server is active
    } while (error == INVALID);

    if (is_connected_app)
    {
        if (redirect) // is setting get->HomeMenu_Redirection enabled
        {
            log_info("Redirect on with app connected");

            error = IPCSendCommand(ENABLE_HOME_REDIRECT, (uint8_t*)&IPC_BUFFER[0]);
            if (error == NO_ERROR)
                log_debug(" HOME MENU REDIRECT IS ENABLED");
            else {
                log_debug("[ItemzDaemon][ERROR] Home menu redirect failed");
                return false;
            }
        }
    }
    else
        return false;

    return true;

}



int Start_IF_internal_Services(int reboot_num)
{
    int ret = 0;
    unlink(ITEMZ_LOG);
    mkdir(APP_PATH(""), 0777);

    /*-- INIT LOGGING FUNCS --*/
    log_set_quiet(false);
    log_set_level(LOG_DEBUG);
    FILE* fp = fopen(ITEMZ_LOG, "w");
    if (fp != NULL)
      log_add_fp(fp, LOG_DEBUG);
    /* -- END OF LOGINIT --*/

    generate_build_time();

    log_info("------------------------ Itemzflow Compiled Time: %s @ %s  -------------------------", __DATE__, __TIME__);
    log_info("---------------------------  Itemzflow Version: %s Num of Reboots: %i----------------------------", completeVersion.c_str(), reboot_num);

    get = &set;

    // internals: net, user_service, system_service
    loadModulesVanilla();
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PAD)) return INIT_FAILED;
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG)) return INIT_FAILED;
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_NETCTL)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_NET)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_HTTP)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SSL)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_COMMON_DIALOG)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIOOUT)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_AUDIOIN)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SYSUTIL)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_BGFT)) return INIT_FAILED;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_APPINSTUTIL)) return INIT_FAILED;
    //if (sceSysmoduleLoadModuleInternal(SCE_SYS)) return INIT_FAILED;
    sceSysmoduleLoadModuleInternal(0x80000026);
    sceSysmoduleLoadModule(0x009A);  // internal FreeType, libSceFreeTypeOl


    //log_info("ALL internal Modules are loaded FT %s", FT_fnc());

    //Dump code and sig hanlder
    struct sigaction new_SIG_action;

    log_info("Registering Signal handler... ");

   // new_SIG_action.sa_sigaction = SIG_IGN;
    new_SIG_action.sa_handler = SIG_Handler;
    sigemptyset(&new_SIG_action.sa_mask);
    new_SIG_action.sa_flags = 0;

    for (int i = 0; i < 43; i++) {
      //  if(i != SIGQUIT)
          sigaction(i, &new_SIG_action, NULL);
    }

    log_info("Registered Handler");

    if(sceNetCtlInit() < 0)
    {
        log_info("sceNetCtlInit failed");
        return INIT_FAILED;
    }


    //USB LOGGING
    mkdir(fmt::format("/mnt/usb{0:d}/itemzflow", usbpath()).c_str(), 0777);
    std::string usb_log = fmt::format("/mnt/usb{0:d}/itemzflow/itemzflow_app.log", usbpath()).c_str();
    if (touch_file(usb_log.c_str()))
	{
        unlink(usb_log.c_str());
		fp = fopen(usb_log.c_str(), "w");
		if (fp != NULL){
			log_add_fp(fp, LOG_DEBUG);
            log_debug("---------- Hello World from USB => ICT: %s @ %s | Ver %s -------", __DATE__, __TIME__, completeVersion.c_str());
        }
	}

    log_info("Initiating Native Dialogs ... ");

    sceCommonDialogInitialize();
    sceMsgDialogInitialize();

    log_info("Initiating Controls... ");

    if (!orbisPadInit()) return INIT_FAILED;

    globalConf.confPad = orbisPadGetConf();

    log_info("initiated Controls");

    if (!orbisAudioInit()) return INIT_FAILED;

    log_info("Initiating Audio... ");

    if (orbisAudioInitChannel(ORBISAUDIO_CHANNEL_MAIN, 1024, 48000, ORBISAUDIO_FORMAT_S16_STEREO) != 0) return INIT_FAILED;

    log_info("initiated Audio");

    //rif_exp("/user/license/freeIV0002-ITEM00001_00.rif");
    mkdir("/user/app/ITEM00001/covers", 0777);//
    mkdir("/user/app/ITEM00001/custom_app_names", 0777);


    log_info("Loading Settings... ");
    if (!LoadOptions(get)) 
        msgok(WARNING, getLangSTR(INI_ERROR));
    else
        log_info("Loaded Settings");

    if(init_curl()){
        log_info("CURL INIT Successfull");
        launch_update_thread();
        log_info("Update Thread Launched");
    }
    else
        log_info("CURL INIT Failed");
    
#ifndef HAVE_SHADER_COMPILER
    if((ret = sceKernelLoadStartModule("/system/common/lib/libScePigletv2VSH.sprx", 0, NULL, 0, NULL, NULL)) >= PS4_OK)
#else
    if ((s_piglet_module = sceKernelLoadStartModule("/data/piglet.sprx", NULL, NULL, NULL, NULL, NULL)) >= PS4_OK &&
        (s_shcomp_module = sceKernelLoadStartModule("/data/compiler.sprx", NULL, NULL, NULL, NULL, NULL)) >= PS4_OK)
#endif

    {

        log_info("Starting SQL Services...");
        log_info("SQL library Version: %s", sqlite3_libversion());
        log_info("Started SQL Services ");

        int BigAppid = sceSystemServiceGetAppIdOfBigApp();
        if ((BigAppid & ~0xFFFFFF) == 0x60000000) {
            log_info("FOUND Open Game: %x : Bool: %d", BigAppid, (BigAppid & ~0xFFFFFF) == 0x60000000);
            gm_p_text[0] = getLangSTR(CLOSE_GAME);
        }
        else
            log_info("No Open Games detected");

        if (get->setting_bools[Daemon_on_start])
        {
            log_info("Starting Daemon... ");
            if (!init_daemon_services(get->setting_bools[HomeMenu_Redirection]))
                ani_notify(NOTIFI_WARNING, getLangSTR(FAILED_DAEMON), getLangSTR(DAEMON_OFF2));
            else
                log_debug(" The Itemzflow init_daemon_services succeeded.");
        }
       else
           ani_notify(NOTIFI_WARNING, getLangSTR(DAEMON_OFF), getLangSTR(DAEMON_OFF2));

        if(MP3_Player(get->setting_strings[MP3_PATH]) != MP3_STATUS_ERROR)
            ani_notify(NOTIFI_SUCCESS, getLangSTR(MP3_PLAYING), getLangSTR(MP3_LOADED));

        sceMsgDialogTerminate();
    }
    else {
#ifndef HAVE_SHADER_COMPILER 
         msgok(FATAL, fmt::format("{} {}\nPS4 Path: /system/common/lib/libScePigletv2VSH.sprx", ret, getLangSTR(PIG_FAIL)));
#else
         msgok(FATAL, "HAS_SHADER_COMP: Piglet (custom) failed to load with 0x%x\nPiglet Path: /data/piglet.sprx, Compiler Path: /data/compiler.sprx", s_piglet_module);
#endif
         return INIT_FAILED;
    }

    // CHECK FOR LOCAL UPDATE FILE
    check_for_update_file(); 

    //dont_show_donate_message
    if (!if_exists("/user/app/ITEM00001/support.flag"))
        ani_notify(NOTIFI_KOFI, getLangSTR(CONSIDER_SUPPORT), "https://ko-fi.com/lightningmods");
    // all fine.
    return PS4_OK;
}