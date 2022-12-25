#include <stdio.h>
#include <stdlib.h>
#include <md5.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <pthread.h>
#include "utils.h"
#include <filesystem>
#include <fstream>
#include <iosfwd>
extern "C"{
#include "mp3/dr_mp3.h"
#include <orbisAudio.h>
#include "ini.h"
#include "log.h"
#include "net.h"
#include "defines.h"
#include "dialog.h"
#include "zip/zip.h"
}
#include "installpkg.h"
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <signal.h>
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
/*================== MISC GOLBAL VAR ============*/
static uint32_t sdkVersion = -1;
static std::string checkedsize;
static const char* sizes[] = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

void* __stack_chk_guard = (void*)0xdeadbeef;
size_t _orbisFile_lastopenFile_size;
/* =========================================== */

/* ================== IF GLOBAL VARS========== */
ItemzSettings set,
* get;

extern char ipc_msg[];
extern std::atomic_int inserted_disc;
extern std::atomic_bool is_disc_inserted;
extern struct retry_t* cf_tex;
extern int total_pages;
extern std::vector<item_t> all_apps; // Installed_Apps
bool sceAppInst_done = false;

bool (*sceStoreApiLaunchStore)(const char* query) = NULL;
update_ret (*sceStoreApiCheckUpdate)(const char* tid) = NULL;
/* =========================================== */

/* ================== MP3 VARS =====================-*/
#include <libtag/taglib.h>
#include <libtag/fileref.h>
#include <libtag/tag.h>
std::vector<item_t> mp3_playlist;
current_song_t current_song;

static uint64_t off2 = 0;
//drmp3_uint8  pData[DRMP3_DATA_CHUNK_SIZE]; // reading buffer from opened file, 16k
static drmp3        mp3;      // the main dr_mp3 object
static drmp3_config Config;   // and its config
static std::atomic_int dataSize = 0;
static std::vector<uint8_t> mp3data;

extern int g_idx;
static std::atomic_int main_ch = 0;
static std::atomic_int fill = 0;
static std::atomic_bool m_bPlaying = false;
static int numframe = 1;
int retry_mp3_count = 0;

static short play_buf[(1024 * 2) * 4], // small fixed buffer to append decoded pcm s16le samples
snd[1152 * 2];      // small fixed buffer to store a single audio frame of decoded samples



#define MP3_BUFFER_SIZE  (1152) // max decoded s16le sample, for a single channel
/*===============================================*/

/*============= USB VARS ========================*/
extern pthread_mutex_t usb_lock;
std::atomic_int usb_number = ATOMIC_VAR_INIT(false);
/*=============================================*/

/*-===================== THEME =========================*/
typedef struct {
    std::string name, version, date, author;
    bool is_image, has_font, using_sb;
} theme;

const char *required_theme_files[] = {
    "theme/btn_o.png", "theme/btn_sq.png",
    "theme/btn_tri.png", "theme/btn_x.png", 
    "theme/btn_up.png", "theme/btn_down.png", 
    "theme/btn_left.png", "theme/btn_right.png",
    "theme/btn_l1.png", "theme/btn_l2.png", 
    "theme/btn_r1.png"
};

const char *if_d_zip[] = {
    "ONLY WORKS IN ITEMZFLOW"
};
bool reboot_app = false;
#include "shaders.h"
/*=====================================================*/
/*
* Creates all the directories in the provided path. (can include a filename)
* (directory must end with '/')
*/

void* prx_func_loader(std::string prx_path, std::string symbol) {

    void* addrp = NULL;
    log_info("Loading %s", prx_path.c_str());
     
    int libcmi = sceKernelLoadStartModule(prx_path.c_str(), 0, nullptr, 0, 0, 0);
    if(libcmi < 0){
        log_error("Error loading prx: 0x%X", libcmi);
        return addrp;
    }
    else
        log_debug("%s loaded successfully", prx_path.c_str());

    if(sceKernelDlsym(libcmi, symbol.c_str(), &addrp) < 0){
        log_error("Symbol %s NOT Found", symbol.c_str());
    }
    else
        log_debug("Function %s | addr %p loaded successfully", symbol.c_str(), addrp);

    return addrp;
}

// folder that sees if a path is a folder
bool is_folder(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode);
    return false;
}

static int theme_info(void* user, const char* section, const char* name,
    const char* value, int unused)
{
    static char prev_section[50] = "";

    if (strcmp("THEME", prev_section)) {
        log_debug("%s [%s]", (prev_section[0] ? "" : ""), section);
        strncpy(prev_section, section, sizeof(prev_section));
        prev_section[sizeof(prev_section) - 1] = '\0';
    }

    theme* set = (theme*)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("THEME", "Author")) {
        set->author = value;
    }
    if (MATCH("THEME", "Date")) {
        set->date = value;
    }
    if (MATCH("THEME", "Name")) {
        set->name = value;
    }
    if (MATCH("THEME", "Version")) {
        set->version = value;
    }
    if (MATCH("THEME", "Image")) {
        set->is_image = atol(value);
    }
    if (MATCH("THEME", "Font")) {
        set->has_font = atol(value);
    }
    if (MATCH("THEME", "Shader") && !set->is_image) {
        set->using_sb = atol(value);
    }

    return  0;
}

void __stack_chk_fail(void)
{
    log_info("Stack smashing detected.");
    msgok(FATAL, "Stack Smashing has been Detected");
}

bool IS_ERROR(uint32_t ret)
{
    return ret & 0x80000000;
}

std::string calculateSize(uint64_t size)
{
    std::string result;
    uint64_t  multiplier = exbibytes;
    int i;

    for (i = 0; i < DIM(sizes); i++, multiplier /= 1024)
    {
        if (size < multiplier)
            continue;
        if (size % multiplier == 0)
            result = fmt::format("{0:} {1:}", size / multiplier, sizes[i]);
        else
            result = fmt::format("{0:.1f} {1:}", (float)(size / multiplier), sizes[i]);

        return result;
    }
    result = "0";
    return result;
}

size_t CalcAppsize(const char* filename) {
    struct stat st;
    if(stat(filename, &st) != 0) {
        return 0;
    }
    return st.st_size;   
}

#if __cplusplus
extern "C" {
#endif
int64_t sys_dynlib_load_prx(char* prxPath, int* moduleID)
{
    return (int64_t)syscall4(594, prxPath, 0, moduleID, 0);
}

int64_t sys_dynlib_unload_prx(int64_t prxID)
{
    return (int64_t)syscall1(595, (void*)prxID);
}


int64_t sys_dynlib_dlsym(int64_t moduleHandle, const char* functionName, void* destFuncOffset)
{
    return (int64_t)syscall3(591, (void*)moduleHandle, (void*)functionName, destFuncOffset);
}

unsigned char* orbisFileGetFileContent(const char* filename)
{
    _orbisFile_lastopenFile_size = -1;

    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        log_error("Unable to open file \"%s\".", filename); return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc((size + 1) * sizeof(char));
    fread(buffer, sizeof(char), size, file);
    buffer[size] = 0;
    _orbisFile_lastopenFile_size = size;
    fclose(file);

    return buffer;
}

int sceSystemServiceKillApp(uint32_t appid, int opt,int method, int reason);

int ItemzLocalKillApp(uint32_t appid) {

    log_info("ItemzLocalKillApp(%x, -1, 0, 0)", appid);
    if ((appid & ~0xFFFFFF) == 0x60000000)
        return sceSystemServiceKillApp(appid, -1, 0, 0);
    else
        return ITEMZCORE_EINVAL;
}

int sceShellUIUtilInitialize();
int sceShellUIUtilLaunchByUri(const char* uri,  SceShellUIUtilLaunchByUriParam *Param);

int ItemzLaunchByUri(const char* uri) {
    int libcmi = -1;

    if (!uri)
        return ITEMZCORE_EINVAL;
        
    if (sys_dynlib_load_prx((char*)"/system/common/lib/libSceShellUIUtil.sprx", &libcmi) < 0 || libcmi < 0) 
        return ITEMZCORE_PRX_RESOLVE_ERROR;

    SceShellUIUtilLaunchByUriParam Param;
    Param.size = sizeof(SceShellUIUtilLaunchByUriParam);
    sceUserServiceGetForegroundUser(&Param.userId); // DONT CARE
    log_info("ItemzLaunchByUri(%s)", uri);
    log_info("sceShellUIUtilInitialize returned %x", sceShellUIUtilInitialize());

    return sceShellUIUtilLaunchByUri(uri, &Param);
}


bool copyFile(std::string source, std::string dest, bool show_progress)
{
    std::string buf;
    int src = sceKernelOpen(source.c_str(), 0x0000, 0);
    if (src > 0)
    {
        
        int out = sceKernelOpen(dest.c_str(), 0x0001 | 0x0200 | 0x0400, 0777);
        if (out > 0)
        {   
            if(show_progress)
                 progstart((char*)"Starting...");

            size_t bytes ,total = 0;
            size_t len = CalcAppsize((char*)source.c_str());
            std::string size = calculateSize(len);

            std::vector<char> buffer(MB(10));
            if (buffer.size() > 0)
            {
                while (0 < (bytes = sceKernelRead(src, &buffer[0], MB(10)))){
                    sceKernelWrite(out, &buffer[0], bytes);
                    total += bytes;
                    if(show_progress){
			           uint32_t g_progress = (uint32_t)(((float)total / len) * 100.f);
			           int status = sceMsgDialogUpdateStatus();
			            if (ORBIS_COMMON_DIALOG_STATUS_RUNNING == status) {
                            buf = fmt::format("{0:}...\n Size: {1:}", getLangSTR(COPYING_FILE), size);
			            	sceMsgDialogProgressBarSetValue(0, g_progress); 
				            sceMsgDialogProgressBarSetMsg(0, buf.c_str());
			            }
                    }
                }
                buffer.clear();
            }
        }
        else
        {
            fmt::print("Could copy file: {} Reason: {x}", src, out);
            sceKernelClose(src);
            if(show_progress)
                 sceMsgDialogTerminate();
            return false;
        }

        sceKernelClose(src);
        sceKernelClose(out);
        sceMsgDialogTerminate();
        return true;
    }
    else
    {
        fmt::print("Could copy file: {} Reason: {x}", dest, src);
        return false;
    }
}

void ProgSetMessagewText(int prog, const char* fmt, ...)
{

        char buff[300];
        memset(&buff[0], 0, sizeof(buff));

        va_list args;
        va_start(args, fmt);
        vsnprintf(&buff[0], 299, fmt, args);
        va_end(args);

        sceMsgDialogProgressBarSetValue(0, prog);
        sceMsgDialogProgressBarSetMsg(0, buff);
}
/// timing
unsigned int get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

bool touch_file(const char* destfile)
{
    int fd = open(destfile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd > 0) {
        unlink(destfile);
        close(fd);
        return true; 
    }
    else
        return false;
}
#define SCE_KERNEL_EVF_WAITMODE_OR 0x02
unsigned char _bittest64(uint64_t *Base, uint64_t Offset)
{
  int old = 0;
  __asm__ __volatile__("btq %2,%1\n\tsbbl %0,%0 "
    :"=r" (old),"=m" ((*(volatile long long *) Base))
    :"Ir" (Offset));
  return (old != 0);
}

unsigned int sceShellCoreUtilIsUsbMassStorageMounted_func(unsigned int usb_index)
{
  uint64_t res;
  void* ef; 

  if ( (unsigned int)sceKernelOpenEventFlag(&ef, "SceAutoMountUsbMass") )
    return 0;
  sceKernelPollEventFlag(ef, 0xFFFFFFF, SCE_KERNEL_EVF_WAITMODE_OR, &res);
  sceKernelCloseEventFlag(ef);

  return _bittest64(&res, usb_index);
}

unsigned int usbpath()
{
    unsigned int usb_index = -1;
    pthread_mutex_lock(&usb_lock);
    for (int i = 0; i < 8; i++){
        if (sceShellCoreUtilIsUsbMassStorageMounted_func((unsigned int)i)){
              usb_index = i;
              //log_info("[UTIL] USB %i is mounted, SceAutoMountUsbMass: %i", i, usb_number);
              break;
        }
    }
    pthread_mutex_unlock(&usb_lock);

    return usb_index;
}

bool MD5_file_compare(const char* file1, const char* file2)
{
	unsigned char c[MD5_HASH_LENGTH];
	unsigned char c2[MD5_HASH_LENGTH];
	int i;
	FILE* f1 = fopen(file1, "rb");
	FILE* f2 = fopen(file2, "rb");
    if(!f1 || !f2)
    {
        log_error("Could not open both files");
        return false;
    }
	MD5_CTX mdContext;

	MD5_CTX mdContext2;
	int bytes2 = 0;
	unsigned char data2[1024];

	int bytes = 0;
	unsigned char data[1024];

	MD5_Init(&mdContext);
	while ((bytes = fread(data, 1, 1024, f1)) != 0)
		MD5_Update(&mdContext, data, bytes);
	MD5_Final(c, &mdContext);

	MD5_Init(&mdContext2);
	while ((bytes2 = fread(data2, 1, 1024, f2)) != 0)
		MD5_Update(&mdContext2, data2, bytes2);
	MD5_Final(c2, &mdContext2);

	for (i = 0; i < 16; i++)
	{
		if (c[i] != c2[i]){
            log_error("MD5 compare failed: %s != %s HASH %s | %s", file1, file2, c, c2);
            fclose(f1);
            fclose(f2);
            return false;
        }
	}

	fclose(f1);
	fclose(f2);

    log_info("MD5s match for %s and %s", file1, file2);

	return true;
}

bool MD5_hash_compare(const char* file1, const char* hash)
{
    unsigned char c[MD5_HASH_LENGTH];
    FILE* f1 = fopen(file1, "rb");
    if(!f1) return false;
    MD5_CTX mdContext;

    int bytes = 0;
    unsigned char data[1024];

    MD5_Init(&mdContext);
    while ((bytes = fread(data, 1, 1024, f1)) != 0)
        MD5_Update(&mdContext, data, bytes);
    MD5_Final(c, &mdContext);

    char md5_string[33] = {0};

    for(int i = 0; i < 16; ++i) {
        snprintf(&md5_string[i*2], 32, "%02x", (unsigned int)c[i]);
    }
    log_info( "[MD5] FILE HASH: %s", md5_string);
    md5_string[32] = 0;

    if (strcmp(md5_string, hash) != 0) {
        fclose(f1);
        return DIFFERENT_HASH;
    }

    log_info( "[MD5] Input HASH: %s", hash);
    fclose(f1);

    return SAME_HASH;
}


bool is_apputil_init()
{
    int ret = sceAppInstUtilInitialize();
    if (ret) {
        log_error( " sceAppInstUtilInitialize failed: 0x%08X", ret);
        return false;
    }
    else
        sceAppInst_done = true;

    return true;
}

int sceAppInstUtilAppUnInstallPat(const char* tid);

bool app_inst_util_uninstall_patch(const char* title_id, int* error) {
    int ret;

    *error = 0;
    if(!sceAppInst_done)
    {
        bool res = is_apputil_init();
        if (!res)
        {  *error = 0xDEADBEEF;
           return false;
        }
    }
    if (!title_id) {
        ret = 0x80020016;
        if (error) {
            *error = ret;
        }
        goto err;
    }

    ret = sceAppInstUtilAppUnInstallPat(title_id);
    if (ret)
    {
        if (error) {
            *error = ret;
        }
        log_error( " sceAppInstUtilAppUnInstallPat failed: 0x%08X", ret);
        goto err;
    }

    return true;

err:
    log_error("app_inst_util_uninstall_patch: 0x%X", error);
    return false;
}

bool app_inst_util_uninstall_game(const char *title_id, int *error)
{
    int ret;
    *error = 0;

    if (strlen(title_id) != 9)
    {
        *error = 0xBADBAD;
        return false;
    }
    if(!sceAppInst_done)
    {
        bool res = is_apputil_init();
        if (!res)
        {  *error = 0xDEADBEEF;
           return false;
        }    
    }

    if (!title_id)
    {
        ret = 0x80020016;
        if (error)
        {
            *error = ret;
        }
        goto err;
    }


    ret = sceAppInstUtilAppUnInstall(title_id);
    if (ret)
    {
        if (error)
        {
            *error = ret;
        }
        log_error( " sceAppInstUtilAppUnInstall failed: 0x%08X", ret);
        goto err;
    }

    return true;

err:
    log_error("app_inst_util_uninstall_game: 0x%X", error);
    return false;
}

int sceAppInstUtilAppUnInstallAddcont(const char* title_id, const char* content_id);

bool app_inst_util_uninstall_ac(const char* content_id, const char* title_id, int* error) {

    *error = 0;

    if (!sceAppInst_done)
    {
        bool res = is_apputil_init();
        if (!res)
        {
            *error = 0xDEADBEEF;
            return false;
        }
    }

    int ret = sceAppInstUtilAppUnInstallAddcont(title_id, content_id);
    if (ret) {
        if (error) {
            *error = ret;
        }
        log_error("sceAppInstUtilAppUnInstallAddcont failed: 0x%08X", ret);
        return false;
    }

    return true;
}


int app_inst_util_get_size(const char* title_id) {
    uint64_t size = 0;

    if (!sceAppInst_done) {
        log_debug(" Starting app_inst_util_init..");
        if (!app_inst_util_init()) {
            log_error("app_inst_util_init has failed...");
            return ITEMZCORE_GENERAL_ERROR;
        }
    }

    int ret = sceAppInstUtilAppGetSize(title_id, &size);
    if (ret) {
        log_error("sceAppInstUtilAppGetSize failed: 0x%08X", ret);
        return ret;
    }
    else
        log_info("Size: %s", calculateSize(size).c_str());
   
    return B2GB(size);
}

bool Fnt_setting_enabled = false;

static int print_ini_info(void* user, const char* section, const char* name,
    const char* value, int unused)
{
    static char prev_section[50] = "";

    if (strcmp(section, prev_section)) {
        log_debug("%s[%s]", (prev_section[0] ? "" : ""), section);
        strncpy(prev_section, section, sizeof(prev_section));
        prev_section[sizeof(prev_section) - 1] = '\0';
    }

    ItemzSettings* set = (ItemzSettings*)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("Settings", "Daemon_on_start")) {
        set->setting_bools[Daemon_on_start] = atoi(value);
    }
    else if (MATCH("Settings", "TTF_Font")) {
        set->setting_strings[FNT_PATH] = value;
        if (set->setting_strings[FNT_PATH].find("SCE-PS3-RD-R-LATIN.TTF") == std::string::npos){
            log_debug("Font is not default, enabling font settings");
            Fnt_setting_enabled = true;
        }
    }
    else if (MATCH("Settings", "HomeMenu_Redirection")) {
        set->setting_bools[HomeMenu_Redirection] = atoi(value);
    }
    else if (MATCH("Settings", "Show_install_prog")) {
        set->setting_bools[Show_install_prog] = atoi(value);
    }
    else if (MATCH("Settings", "Dumper_option")) {
        set->Dump_opt = (Dump_Multi_Sels)atoi(value);
    }
    else if (MATCH("Settings", "Sort_By")) {
        set->sort_by = (Sort_Multi_Sel)atoi(value);
    }
    else if (MATCH("Settings", "Sort_Cat")) {
        set->sort_cat = (Sort_Category)atoi(value);
    }
    else if (MATCH("Settings", "cover_message")) {
       set->setting_bools[cover_message] = atoi(value);
    }
    else if (MATCH("Settings", "Dumper_Path")) {
        set->setting_strings[DUMPER_PATH] = value;
    }
    else if (MATCH("Settings", "MP3_Path")) {
        set->setting_strings[MP3_PATH] = value;
    }
    else if (MATCH("Settings", "Show_Buttons")) {
        set->setting_bools[Show_Buttons] = atoi(value);
    }
    else if (MATCH("Settings", "Enable_Theme")) {
        set->setting_bools[using_theme] = atoi(value);
        log_debug("Enable_Theme: %d : %d", set->setting_bools[using_theme], atoi(value));
   }
    else if (MATCH("Settings", "Image_path")) {
       set->setting_strings[IMAGE_PATH] = value;//nReflections
    }
    else if (MATCH("Settings", "Reflections")) {
        use_reflection = atoi(value);
    }
   
    return 1;
}


bool LoadOptions(ItemzSettings *set)
{
    bool no_error = true;
    int error = 1;
    theme t;
    unsigned int usb_num = usbpath();

    set->setting_bools.reserve(NUMB_OF_BOOLS);
    set->setting_strings.reserve(NUMB_OF_STRINGS);

    //"http://IFUpdate.pkg-zone.com"
    dump = SEL_RESET;
    set->setting_bools[Daemon_on_start] = true;
    set->Dump_opt = SEL_DUMP_ALL;
    set->sort_cat = NO_CATEGORY_FILTER;
    set->sort_by = NA_SORT;
    set->setting_bools[using_sb] = true;
    set->setting_bools[Show_install_prog] = true;
    set->setting_bools[Show_Buttons] = true;
    set->setting_bools[HomeMenu_Redirection] = false;
    set->setting_bools[cover_message] = true;
    set->ItemzCore_AppId = sceSystemServiceGetAppIdOfMiniApp();
    set->ItemzDaemon_AppId = -1;
    if(usb_num != -1)
       set->setting_strings[DUMPER_PATH] =  fmt::format("/mnt/usb{}/", usb_num);
    else
       set->setting_strings[DUMPER_PATH] =  "/mnt/usb0";


    set->setting_strings[FNT_PATH] =  "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF";
    /* Initialize INI structure */
    if (usb_num != -1)
    {
        set->setting_strings[INI_PATH] = fmt::format("{}/settings.ini", set->setting_strings[DUMPER_PATH]);
        if (!if_exists(set->setting_strings[INI_PATH].c_str()))
        {
            log_warn("No INI on USB");
            if (if_exists("/user/app/ITEM00001/settings.ini")){
                set->setting_strings[INI_PATH] = "/user/app/ITEM00001/settings.ini";
                error = ini_parse("/user/app/ITEM00001/settings.ini", print_ini_info, set);
                if (error < 0) log_error("Bad config file (first error on line %d)!", error);
            }
        } else {
            error = ini_parse(set->setting_strings[INI_PATH].c_str(), print_ini_info, set);
            if (error < 0) log_error("Bad config file (first error on line %d)!", error);
            log_info( " Loading ini from USB");
        }
    }
    else if (!if_exists("/user/app/ITEM00001/settings.ini"))
    {
        log_error("CANT FIND INI"); no_error = false;
    } else {
        error = ini_parse("/user/app/ITEM00001/settings.ini", print_ini_info, set);
        if (error < 0) log_error("Bad config file (first error on line %d)!", error);
        log_info( " Loading ini from APP DIR");
        set->setting_strings[INI_PATH] = "/user/app/ITEM00001/settings.ini";
    }

    if(set->setting_bools[using_theme] && (set->setting_bools[using_theme] = if_exists(APP_PATH("theme/theme.ini")))){
    log_debug("[THEME] Using Theme detected");
    log_debug("[THEME] Getting Theme Info");

     t.is_image = false;
     t.using_sb = false;
     t.has_font = false;
     if(ini_parse(APP_PATH("theme/theme.ini"), theme_info, &t) < 0){
        log_error("[THEME] Failed to get Theme Info");
        set->setting_bools[using_theme] = false;
        goto no_theme;
     } 

    log_debug("[THEME] ======== Theme Info =======");

    fmt::print("[THEME] Name: {}", t.name);
    fmt::print("[THEME] Version: {}", t.version);
    fmt::print("[THEME] Author: {}", t.author);
    fmt::print("[THEME] Date: {}", t.date);

    log_debug("[THEME] Image: %s", t.is_image ? "Yes" : "No");
    log_debug("[THEME] Font: %s", t.has_font ? "Yes" : "No");
    log_debug("[THEME] Shader: %s", t.using_sb ? "Yes" : "No");
    log_debug("[THEME] ========  End of Theme Info =======");
    log_debug("[THEME] Setting ItemzCore settings ...");

    if(t.has_font){
      //THEME FONT CHECK
       set->setting_strings[FNT_PATH] = fmt::format("{}/font.ttf", APP_PATH("theme"));
       if(!if_exists(set->setting_strings[FNT_PATH].c_str())){
          log_error("[THEME] Font not found");
          t.has_font = false;
       }
       else{
         log_debug("[THEME] Font found");
         Fnt_setting_enabled = true;
       }
    }
    else
       log_debug("[THEME] No Font being used");

    if(t.is_image){
        set->setting_strings[IMAGE_PATH] = fmt::format("{}/background.png", APP_PATH("theme"));
        if(!if_exists( set->setting_strings[IMAGE_PATH].c_str())){
          fmt::print("[THEME] Image not found, looked @ {}",  set->setting_strings[IMAGE_PATH]);
          set->setting_bools[has_image] = t.is_image = false;
        }
        else{
           fmt::print("[THEME] Image found @ {}",  set->setting_strings[IMAGE_PATH]);
           set->setting_bools[has_image] = true;
           set->setting_bools[using_sb] = false;
        }
    }
    else {
       log_debug("[THEME] No Image being used");
       set->setting_strings[SB_PATH] = fmt::format("{}/shader.bin", APP_PATH("theme"));
       if(!if_exists(set->setting_strings[SB_PATH].c_str())){
          log_error("[THEME] Shader not found");
          set->setting_bools[using_sb] = t.using_sb = false;
       }
       else{
           log_debug("[THEME] Shader found");
           set->setting_bools[using_sb] = true;
           set->setting_bools[has_image] = false;
       }
    }

    log_debug("[THEME] Freeing Theme ...");
    }
no_theme:
    if(!set->setting_strings[IMAGE_PATH].empty() 
    && !set->setting_bools[has_image]){
       set->setting_bools[has_image] = true;
       set->setting_bools[using_sb] = false;
    }
    log_debug("Starting Lang.");
    uint32_t lang = PS4GetLang();
    if (error < 0) 
        log_error("ERROR reading INI setting default vals");

#if TEST_INI_LANGS
    for (int i = 0; i < 29; i++)
    {
        if (!LoadLangs(i))
            log_debug("Failed to Load %s", Language_GetName(i));
        else
            log_debug("Successfully Loaded %s", Language_GetName(i));
        
    }
#endif
    //fallback lang
#ifdef OVERRIDE_LANG
    lang = OVERRIDE_LANG;
#else
    set->lang = lang;
#endif

    if (!LoadLangs(lang)) {
        set->lang = 0x01; /// fallback english
        if (!LoadLangs(set->lang)) {
            log_debug(" This PKG has no lang files... trying embedded");

            if(!load_embdded_eng())
               msgok(FATAL, "Failed to load Backup Lang...\nThe App is unable to continue");
            else
              log_debug(" Loaded embdded ini lang file");
        }
        else
            log_debug(" Loaded the backup, %s failed to load", Language_GetName(lang));
    }

    if (!Fnt_setting_enabled) {

        switch (lang) {
        case 0: //jAPN IS GREAT
            set->setting_strings[FNT_PATH] = asset_path("fonts/NotoSansJP-Regular.ttf");
            break;
        case 9:///THIS IS FOR JOON, IF HE COMES BACK
            set->setting_strings[FNT_PATH] = asset_path("fonts/NotoSansKR-Regular.ttf");
            break;
        case 21:
        case 23:
        case 24:
        case 25:
        case 26:
            set->setting_strings[FNT_PATH] = asset_path("fonts/HelveticaWorld-Multi.ttf");
            break;
            //NO USERS HAS TRANSLATED THE FOLLOWING LANGS, SO I DELETED THEM OUT THE PKG TO SAVE SPACE
            /*
            case 10:
                strcpy(set->opt[FNT_PATH],"/mnt/sandbox/pfsmnt/ITEM00001-app0/assets/fonts/NotoSansTC-Regular.ttf");
                break;
            case 11:
                strcpy(set->opt[FNT_PATH],"/mnt/sandbox/pfsmnt/ITEM00001-app0/assets/fonts/NotoSansSC-Regular.ttf");
                break;
            case 27:
                strcpy(set->opt[FNT_PATH],"/mnt/sandbox/pfsmnt/ITEM00001-app0/assets/fonts/NotoSansThai-Regular.ttf");
                break;*/
        default:
            break;
        }
    }
    //

    fmt::print("set->ini_path: {}", set->setting_strings[INI_PATH]);
    fmt::print("set->dumper_path: {}", set->setting_strings[DUMPER_PATH]);
    fmt::print("get->fnt_path: {}", set->setting_strings[FNT_PATH]);

    log_info( "set->Daemon_...   : %s", set->setting_bools[Daemon_on_start] ? "ON" : "OFF");
    log_info( "set->HomeMen...   : %s", set->setting_bools[HomeMenu_Redirection] ? "ItemzFlow (ON)" : "Orbis (OFF)");
    log_info( "set->sort_by      : %i", set->sort_by);
    log_info( "set->Cover_message : %i", set->setting_bools[Cover_message]);
    log_info( "set->using_theme  : %i", set->setting_bools[using_theme]);
    log_info( "set->Dump_opt     : %i", set->Dump_opt);
    log_info( "set->Install_prog.: %s", set->setting_bools[Show_install_prog] ? "ON" : "OFF");
    log_info( "set->Lang         : %s : %i", Language_GetName(lang), set->lang);

    return no_error;
}


bool SaveOptions(ItemzSettings *set)
{  
    std::string ini_content = fmt::format("[Settings]\nSort_By={0:d}\nSort_Cat={1:d}\nSecure_Boot=1\nTTF_Font={2:}\nShow_install_prog={3:d}\nHomeMenu_Redirection={4:d}\nDaemon_on_start={5:d}\ncover_message={6:d}\nDumper_Path={7:}\nMP3_Path={8:}\nShow_Buttons={9:d}\nEnable_Theme={10:d}\nImage_path={11:}\nReflections={12:d}", (int)set->sort_by, (int)set->sort_cat, set->setting_strings[FNT_PATH], set->setting_bools[Show_install_prog],  set->setting_bools[HomeMenu_Redirection],  set->setting_bools[Daemon_on_start],  set->setting_bools[cover_message], set->setting_strings[DUMPER_PATH], set->setting_strings[MP3_PATH],  set->setting_bools[Show_Buttons],  set->setting_bools[using_theme], set->setting_strings[IMAGE_PATH], use_reflection);

    std::ofstream out(set->setting_strings[INI_PATH]);
    if(out.bad()){
       log_error("Failed to create ini file");
       return false;
    }
    
    out << ini_content;
    out.close();

    log_info("============  Saving the following settings ============");
    fmt::print("set->ini_path: {}", set->setting_strings[INI_PATH]);
    fmt::print("set->dumper_path: {}", set->setting_strings[DUMPER_PATH]);
    fmt::print("get->fnt_path: {}", set->setting_strings[FNT_PATH]);
    fmt::print("set->mp3_path: {}", set->setting_strings[MP3_PATH]);

    log_info("set->Daemon_...   : %s", set->setting_bools[Daemon_on_start] ? "ON" : "OFF");
    log_info("set->HomeMen...   : %s", set->setting_bools[HomeMenu_Redirection] ? "ItemzFlow (ON)" : "Orbis (OFF)");
    log_info("set->sort_by      : %i", set->sort_by);
    log_info("set->Show_Buttons : %i", set->setting_bools[Show_Buttons]);
    log_info("set->cover_message : %i", set->setting_bools[cover_message]);
    log_info("set->Dump_opt     : %i", set->Dump_opt);
    log_info("set->using_theme  : %i", set->setting_bools[using_theme]);
    log_info("set->Install_prog.: %s", set->setting_bools[Show_install_prog] ? "ON" : "OFF");
    log_info("set->Lang         : %s : %i", Language_GetName(set->lang), set->lang);
    log_info("==========================================================");

    /* Load values */
    chmod(set->setting_strings[INI_PATH].c_str(), 0777);

    return true;
}

uint32_t ps4_fw_version(void)
{
    //cache the FW Version
    if (0 < sdkVersion) {
        size_t   len = 4;
        // sysKernelGetLowerLimitUpdVersion == machdep.lower_limit_upd_version
        // rewrite of sysKernelGetLowerLimitUpdVersion
        sysctlbyname("machdep.lower_limit_upd_version", &sdkVersion, &len, NULL, 0);
    }

    // FW Returned is in HEX
    return sdkVersion;
}

//////////////////////////////////////////////////////

bool Keyboard(const char* Title, const char* initialTextBuffer, char* out_buffer)
{
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);

    wchar_t title[100];
    wchar_t inputTextBuffer[255];
    char    titl[100];

    if (initialTextBuffer
        && strlen(initialTextBuffer) > 1023) return "Too Long";

    memset(&inputTextBuffer[0], 0, 255);
    memset(out_buffer, 0, 255);

    if (initialTextBuffer)
        snprintf(out_buffer, 254, "%s", initialTextBuffer);
    //converts the multibyte string src to a wide-character string starting at dest.
    mbstowcs(&inputTextBuffer[0], out_buffer, strlen(out_buffer) + 1);
    // use custom title
    snprintf(&titl[0], 99, "%s", Title ? Title : "Itemzflow");
    //converts the multibyte string src to a wide-character string starting at dest.
    mbstowcs(title, titl, strlen(titl) + 1);

    OrbisImeDialogParam param;
    memset(&param, 0, sizeof(OrbisImeDialogParam));

    param.maxTextLength = 1023;
    param.inputTextBuffer = inputTextBuffer;
    param.title = title;
    param.userId = 0xFE;
    param.type = ORBIS_IME_TYPE_BASIC_LATIN;
    param.enterLabel = ORBIS_IME_ENTER_LABEL_DEFAULT;

    sceImeDialogInit(&param, NULL);

    int status;
    while (1)
    {
        status = sceImeDialogGetStatus();

        if (status == ORBIS_IME_DIALOG_STATUS_FINISHED)
        {
            OrbisImeDialogResult result;
            memset(&result, 0, sizeof(OrbisImeDialogResult));
            sceImeDialogGetResult(&result);
            log_info("status %i, endstatus %i ", status, result.endstatus);

            if (result.endstatus == ORBIS_IME_DIALOG_END_STATUS_USER_CANCELED){
                log_info("User Cancelled this Keyboard Session");
                ani_notify(NOTIFI_WARNING, getLangSTR(OP_CANNED), getLangSTR(OP_CANNED_1));
            }
            else if (result.endstatus == ORBIS_IME_DIALOG_END_STATUS_OK)
            {
                log_info("User Entered text.. Entering into buffer");
                wcstombs(out_buffer, &inputTextBuffer[0], 254);
                sceImeDialogTerm();
                return true;
            }
            goto Finished;
        }
        else if (status == ORBIS_IME_DIALOG_STATUS_NONE) goto Finished;
    }

Finished:
    sceImeDialogTerm();
    return false;
}


bool rmtree(const char path[]) {

    struct dirent* de;
    char fname[300];
    DIR* dr = opendir(path);
    if (dr == NULL)
    {
        log_debug("No file or directory found: %s", path);
        return false;
    }
    while ((de = readdir(dr)) != NULL)
    {
        int ret = -1;
        struct stat statbuf;
        snprintf(fname, 299, "%s/%s", path, de->d_name);
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        if (!stat(fname, &statbuf))
        {
            if (S_ISDIR(statbuf.st_mode))
            {
                log_debug("removing dir: %s Err: %d", fname, ret = unlinkat(dirfd(dr), fname, AT_REMOVEDIR));
                if (ret != 0)
                {
                    rmtree(fname);
                    log_debug("Err: %d", ret = unlinkat(dirfd(dr), fname, AT_REMOVEDIR));
                }
            }
            else
            {
                log_debug("Removing file: %s, Err: %d", fname, unlink(fname));
            }
        }
    }
    closedir(dr);

    return true;
}

bool copy_dir(const char* sourcedir, const char* destdir) {

    log_info("s: %s d: %s", sourcedir, destdir);
    DIR* dir = opendir(sourcedir);

    struct dirent* dp;
    struct stat info;
    char src_path[1024];
    char dst_path[1024];

    if (!dir) {
        return false;
    }
    mkdir(destdir, 0777);
    while ((dp = readdir(dir)) != NULL) {
        if(strstr(dp->d_name, "save.ini") != NULL)
           log_info("Save.ini file skipped...");
        else if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..") 
        || strstr(dp->d_name, "nobackup") != NULL) {
            continue;
        }
        else {
            sprintf(src_path, "%s/%s", sourcedir, dp->d_name);
            sprintf(dst_path, "%s/%s", destdir, dp->d_name);

            if (!stat(src_path, &info)) {
                if (S_ISDIR(info.st_mode)) {
                    copy_dir(src_path, dst_path);
                }
                else if (S_ISREG(info.st_mode)) {
                    if (!copyFile(std::string(src_path), std::string(dst_path), false))
                        return false;
                }
            }
        }
    }
    closedir(dir);

    return true;
}

int malloc_stats_fast(LibcMallocManagedSize *ManagedSize);

void print_memory()
{
   size_t sz = 0;
   LibcMallocManagedSize ManagedSize;
   SCE_LIBC_INIT_MALLOC_MANAGED_SIZE(ManagedSize);
   malloc_stats_fast(&ManagedSize);
   sceKernelAvailableFlexibleMemorySize(&sz);
   double dfp_fmem = (1. - (double)sz / (double)0x8000000) * 100.;
   
   log_info("[MEM] CurrentSystemSize: %s, CurrentInUseSize: %s", calculateSize(ManagedSize.currentSystemSize).c_str(), calculateSize(ManagedSize.currentInuseSize).c_str());
   log_info("[MEM] MaxSystemSize: %s, MaxInUseSize: %s", calculateSize(ManagedSize.maxSystemSize).c_str(), calculateSize(ManagedSize.maxInuseSize).c_str());
   log_info("[VRAM] VRAM_Available: %s, AmountUsed: %.2f%%",calculateSize(sz).c_str(), dfp_fmem);
}

void Kill_BigApp(GameStatus &status){
    int ret = -1;
    
    int BigAppid = sceSystemServiceGetAppIdOfBigApp();
    if(status == NO_APP_OPENED || status == APP_NA_STATUS)
    {
        log_info("[CLOSE] No App is opened");
        return;
    }
    log_info("[CLOSE] BigApp ID: 0x%X", BigAppid);
    if ((ret = ItemzLocalKillApp(BigAppid)) == ITEMZCORE_SUCCESS)
    {
           ani_notify(NOTIFI_INFO, getLangSTR(APP_CLOSED), "");
           log_info("[CLOSE] App Closed");
    }
    else
    {
        ani_notify(NOTIFI_WARNING, getLangSTR(FAILED_TO_CLOSE), fmt::format( "{0:}: {1:#x}", getLangSTR(ERROR_CODE), ret));
        log_error("[CLOSE] Failed to close APP: 0x%X", ret);
    }
    
    status = NO_APP_OPENED;
}

int progstart(char* format, ...)
{

    
    int ret = 0;

    char buff[1024];

    memset(buff, 0, 1024);

    va_list args;
    va_start(args, format);
    vsprintf(&buff[0], format, args);
    va_end(args);

    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);
    sceMsgDialogInitialize();

    OrbisMsgDialogParam dialogParam;
    OrbisMsgDialogParamInitialize(&dialogParam);
    dialogParam.mode = ORBIS_MSG_DIALOG_MODE_PROGRESS_BAR;

    OrbisMsgDialogProgressBarParam  progBarParam;
    memset(&progBarParam, 0, sizeof(OrbisMsgDialogProgressBarParam));

    dialogParam.progBarParam = &progBarParam;
    dialogParam.progBarParam->barType = ORBIS_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE;
    dialogParam.progBarParam->msg = &buff[0];

    sceMsgDialogOpen(&dialogParam);

    return ret;
}

int Confirmation_Msg(std::string msg)
{

    sceMsgDialogTerminate();
    //ds

    sceMsgDialogInitialize();
    OrbisMsgDialogParam param;
    OrbisMsgDialogParamInitialize(&param);
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

    OrbisMsgDialogUserMessageParam userMsgParam;
    memset(&userMsgParam, 0, sizeof(userMsgParam));
    userMsgParam.msg = msg.c_str();
    userMsgParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_YESNO;
    param.userMsgParam = &userMsgParam;
    // cv   
    if (0 < sceMsgDialogOpen(&param))
        return NO;

    OrbisCommonDialogStatus stat;
    //
    while (1) {
        stat = sceMsgDialogUpdateStatus();
        if (stat == ORBIS_COMMON_DIALOG_STATUS_FINISHED) {

            OrbisMsgDialogResult result;
            memset(&result, 0, sizeof(result));

            sceMsgDialogGetResult(&result);

            return result.buttonId;
        }
    }
    //c
    return NO;
}
// now read frames and decode to s16le samples, from callback
static int dr_mp3_decode(void)
{
    if (mp3.pData == NULL) return -1;

    if (mp3.streamCursor <= dataSize.load())
    {
        int count = drmp3_read_pcm_frames_s16(&mp3, MP3_BUFFER_SIZE, &snd[0] /* &snd */);
        if (count < 0) {
            log_info("[MP3] Failed to read from file");
            return 0;
        }
        else if (count == 0) {
            memset(&snd, 0, MP3_BUFFER_SIZE * 2 * sizeof(short)); 
            return -1;
        }
        off2 += count;

        /* play audio sample after being decoded */
        if (!(mp3.streamCursor % 1000)) {
            log_info( "[MP3] audioout play %4d pFrames: %p, %zub %zu %zu %lu %.3f%%",
                count, &snd[0], MP3_BUFFER_SIZE * 2 * sizeof(short),
                off2, dataSize.load(), mp3.streamCursor,
                (float)((float)mp3.streamCursor / (float)dataSize.load()) * 100.f);
        }
    }
    return off2;
}
static void audio_PlayCallback(OrbisAudioSample* _buf2, unsigned int length, void* pdata)
{
    const short* _buf = (short*)_buf2;
    const int     play_size = length * 2 * sizeof(short);
    int  handle = orbisAudioGetHandle(main_ch.load());

    if (m_bPlaying.load())
    {
        static int decoded;
        while (fill.load() < (1024 * 2)){

            decoded = dr_mp3_decode();
            if (decoded < 1) {
                if (!(numframe % 500)) 
                    log_info("[MP3] Playing, but no audio samples decoded yet...");

                memset(&snd[0], 0, sizeof(snd)); // fill with silence
                log_info("[MP3] ended");
                m_bPlaying = false; // break main loop
                mp3data.clear();
                break;
            }
            else {
                memcpy(&play_buf[fill.load()], &snd[0], 4608);  // we are doing in two step, sort of waste
                fill += 1152 /* decoded samples */ * 2 /* channels */;
            }
        }
        if (!(numframe % 500)) 
            log_info("[MP3] play_buf samples: %d", fill.load());

        if (!m_bPlaying.load()) {
            orbisAudioSetCallback(main_ch.load(), 0, 0);
            return;
        }

        /* write to audio device buffers */
        memcpy((void*)_buf, &play_buf[0], play_size); // 1024 samples per channel, = 4096b
        /* move remaining samples to head of buffer */
        memcpy(&play_buf[0], &play_buf[1024 * 2], sizeof(play_buf) - play_size);
        fill -= 1024 * 2; 

        if (fill.load() < 0){

            fill = 0;
            if (!(numframe % 500)) 
                log_info("[MP3] no audio frames to play, fill: %d", fill.load());

            log_info("[MP3] fill.load() < 0");
            m_bPlaying = false;
            mp3data.clear();
        }
    }
    else // Not Playing, so clear buffer
    {
        if (!(numframe % 60)) 
            log_info("[MP3] Inside audio_PlayCallback, not playing m_bPlaying: %d", m_bPlaying.load()); 

        /* write silence to audio device buffers */
        memset((void*)_buf, 0, play_size);
        usleep(1000);
    }
    numframe++;
    sceAudioOutOutput(handle, NULL);
}

/*================================== THEME SHIT BELOW =====================================*/

int on_extract_entry(const char *filename, void *arg)
{
    log_debug("[ZIP] Extracted: %s", filename);
    return 0;
}

bool extract_zip(const char* zip_file, const char* dest_path)
{
	uint64_t progress[2];

	struct zip_t *archive = zip_open(zip_file, ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
	if (!archive){
        log_error("[ZIP] Failed to open archive: %s", zip_file);
		return false;
    }

	progress[0] = 0;
	progress[1] = zip_entries_total(archive);
	zip_close(archive);

	log_debug("[ZIP] Extracting ZIP (%lu files) to %s...", progress[1], dest_path);

	if(zip_extract(zip_file, dest_path, on_extract_entry, progress) == 0) return true;

	return false;
}

/*============================================================================================*/
bool power_settings(Power_control_sel sel)
{

if(Confirmation_Msg(fmt::format("{}?", power_menu_get_str(sel))) == YES){
#define SYSCORE_PID 1
    int num = -1;
    switch(sel){
        case POWER_OFF_OPT:
            num = 31;
            break;
        case RESTART_OPT:
            num = 30;
            break;
        case SUSPEND_OPT:
            num = 1;
            break;
    default:
        return false;
    }
	if (syscall(37, SYSCORE_PID, num) != ITEMZCORE_SUCCESS) return false;
}

	return true;

}
void* timer_thr(void* args){
    sleep(5);
    log_debug("[TIMER] Rebooting App");
    raise(SIGQUIT);
    return NULL;
}

void relaunch_timer_thread()
{
  pthread_t id;
  reboot_app = true;
  pthread_create(&id, NULL, timer_thr, NULL);
}

/*====================================  UPDATE SHIT  ===========================================*/
update_ret check_update_from_url(const char* tid = ITEMZFLOW_TID)
{
    sceStoreApiCheckUpdate = (update_ret(*)(const char*))prx_func_loader(asset_path("../Media/store_api.prx"), "sceStoreApiCheckUpdate");
    if(sceStoreApiCheckUpdate)
    {           
        return sceStoreApiCheckUpdate(tid);
    }
    else
    {
        log_error("Failed to load store_api.prx");
        return IF_UPDATE_ERROR;
    }
}

void* CheckForUpdateThread(void*)
{
    while (1) { 
        update_ret ret = check_update_from_url(ITEMZFLOW_TID);
        if (ret == IF_UPDATE_FOUND){
            log_info(" Update is available, the server reported a different Hash");
            ani_notify(NOTIFI_INFO, getLangSTR(OPT_UPDATE), "");
            sleep(5 * 60);
        }
        else if(ret == IF_NO_UPDATE) {
            sleep(5 * 60); // no update, check again in 5 minutes
        }
        else if(ret == IF_UPDATE_ERROR) {
            log_info("Update Server is connectionn error, retrying in 60 secs");
            sleep(60);
        }
    }

    return 0;
}

void launch_update_thread()
{
    pthread_t id;
    pthread_create(&id, NULL, CheckForUpdateThread, NULL);//  pthread_create(&id, NULL, timer_thr, NULL);
}

#if __cplusplus
}
#endif
bool Launch_Store_URI()
{
    sceStoreApiLaunchStore = (bool (*)(const char*))prx_func_loader(asset_path("../Media/store_api.prx"), "sceStoreApiLaunchStore");
    if(sceStoreApiLaunchStore)
    {
       return sceStoreApiLaunchStore("Itemzflow");     
    }
    else
    {
        log_error("Failed to load store_api.prx");
        return false;
    }

    return false;
}

mp3_status_t MP3_Player(std::string path) {
   
   TagLib::FileRef songf;
   std::ifstream file;
    
    if(path.empty() || path.size() < 3 || !if_exists(path.c_str()))
       return MP3_STATUS_ERROR;
    

    if (m_bPlaying.load() || retry_mp3_count >= 2) // ALREADY PLAYING
       return MP3_STATUS_PLAYING;

    current_song.song_title.clear();
    current_song.song_artist.clear();
    current_song.is_playing = false;
    dataSize = 0;
    if (path.substr(path.size() - 3, 3) != "mp3" && mp3_playlist.empty()) {
         if(!is_folder(path.c_str()))
         {
            log_error("Not a folder: %s", path.c_str());
            goto err;
         }
         FS_FILTER filter = fs_ret.filter;
         fs_ret.filter =  FS_MP3_ONLY;
         index_items_from_dir_v2(path, mp3_playlist);
         fs_ret.filter = filter;
         //skip reserved index
         if (mp3_playlist.size() <= 1) {
            log_error("No MP3s in: %s", path.c_str());
            mp3_playlist.clear();
            goto err;
         }
         else {
           log_info("Found %d MP3 files in %s", mp3_playlist.size()-1, path.c_str());
           current_song.total_files = mp3_playlist.size()-1;
           current_song.current_file = 1;
         }
    }

    if (path.substr(path.size() - 3, 3) != "mp3" && mp3_playlist.size() > 1) {
        if (current_song.current_file >= mp3_playlist.size()) {
            log_info("Reached end of playlist, resetting index");
            current_song.current_file =  1;
        }
        path = path + "/" + mp3_playlist[current_song.current_file].id;
        if (path.substr(path.size() - 3, 3) != "mp3") { 
            log_error("Not a valid MP3 file: %s", path.c_str());
            goto err;
        }
    }
    else if (path.substr(path.size() - 3, 3) == "mp3") {
        current_song.total_files = 1;
        current_song.current_file = 0;
    }
   
   log_info("[MP3] Trying to play: %s", path.c_str());
   songf = TagLib::FileRef(path.c_str());
   if(songf.isNull() || !songf.tag())
   {
      log_error("Unknown File: %s", path.c_str());
      goto err;
   }

    if (path.substr(path.size() - 3, 3) == "mp3") {
        current_song.song_title = songf.tag()->title().isEmpty() ? path : songf.tag()->title().toCString(true);
        current_song.song_artist = songf.tag()->artist().isEmpty() ? "Unknown Artist" : songf.tag()->artist().toCString(true);
    }
    else
    {
        current_song.song_title = songf.tag()->title().isEmpty() ? mp3_playlist[current_song.current_file].id : songf.tag()->title().toCString(true);
        current_song.song_artist = songf.tag()->artist().isEmpty() ? "Unknown Artist" : songf.tag()->artist().toCString(true);
    }

    Config.outputSampleRate = 48000;
    //load mp3 data for the drmp3 mp3 decoder
    file.open(path.c_str(), std::ios::binary);
    if(!file.is_open())
    {
        log_error("Failed to open file: %s", path.c_str());
        goto err;
    }
    file.seekg(0, std::ios::end);
    dataSize = file.tellg();
    file.seekg(0, std::ios::beg);
     //get the file size
    if(dataSize == 0 || dataSize > MB(20))
    {
        log_error("MP3 Too big or is truncated: %s", path.c_str());
        file.close();
        goto err;
    }
    mp3data = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    file.close();
    
    if (!drmp3_init_memory(&mp3, mp3data.data(), mp3data.size(), &Config)) {
        mp3data.clear();
        log_error("[MP3] drmp3_init_memory failed: %s", path.c_str());
        goto err;
    }
   // attach audio callback
   off2 = 0, fill = 0;
   orbisAudioSetCallback(main_ch.load(), audio_PlayCallback, 0);
   // start audio callback, and play
   fmt::print("Playing: {0:} - {1:} ({2:})", current_song.song_title, current_song.song_artist, dataSize, (get->lang == 1 || get->lang == 2));
   current_song.is_playing =  m_bPlaying = true, orbisAudioResume(0);
   current_song.current_file++;
   GLES2_refresh_sysinfo();

   return MP3_STATUS_STARTED;

err:
   if (2 >= retry_mp3_count) {
       ani_notify(NOTIFI_WARNING, getLangSTR(FAILED_MP3), fmt::format("({0:d}/2)", retry_mp3_count+1));
       retry_mp3_count++;
   }
   dataSize = 0;
   mp3data.clear();

   return MP3_STATUS_ERROR;
}

void Stop_Music(){

    if(!m_bPlaying)
        return;

    log_info("Stopping MP3 Playback ...");
    // set playing to false
    current_song.is_playing = m_bPlaying = false;
    //Stop audio callback
    orbisAudioPause(0);
    orbisAudioSetCallback(main_ch.load(), NULL, 0);
    //Clear mp3 data
    mp3data.clear();
    mp3_playlist.clear();
    get->setting_strings[MP3_PATH].clear();
    //Clear mp3 decoder
    drmp3_uninit(&mp3);
    //Clear mp3 tags AND audio channel handle
    current_song.song_artist = current_song.song_title = "";
    retry_mp3_count = current_song.current_file = main_ch = current_song.total_files = 0;

}
bool do_update(std::string url){

    log_debug("[UPDATE] Starting Update...");   
    log_debug("[UPDATE] Updating...");

    if(!is_connected_app){
        log_debug("[UPDATE] Daemon Not connected to app, exiting update mode ... ");
        return false;
    }

#if UPDATE_TESTING==1
    std::string tmp;
    long ret = -1;
    char ipc_msg[100];

    tmp =  fmt::format("{}/update/if_update.pkg", url.c_str());
    if((ret = dl_from_url(tmp.c_str(), APP_PATH("../if_update.pkg"))) < 0){
        log_error("[UPDATE] Failed to download if_update.pkg ret: %i", ret);
        return false;
    }

    log_debug("[UPDATE] Verified, Sending IPC Command for Update Installing ...");
    sprintf(&ipc_msg[1], "%i", getpid());
    log_info("Sending PID: %s", &ipc_msg[1]);
    int error = IPCSendCommand(INSTALL_IF_UPDATE, (uint8_t*)&ipc_msg[0]);
    if (error != NO_ERROR) {
        sceMsgDialogTerminate();
        log_error("[UPDATE] IPC INSTALL UPDATE ERROR: %i", error);
        return false;
    }

    log_debug("[UPDATE] Copied, Removing Temp Files");
    log_debug("[UPDATE] Complete...");

    raise(SIGQUIT);
#else
#ifdef __ORBIS__
    if(!Launch_Store_URI()){
        log_error("[UPDATE] Failed to launch store uri");
        return false;
    }
#endif
#endif
    return true;
}

void check_for_update_file(){
    
    loadmsg(getLangSTR(CHECH_UPDATE_IN_PROGRESS));

    bool found = false;
    log_debug("[STARTUP][UPDATE] Checking for update file ...");
    if(if_exists("/user/update_tmp")){
		log_debug("[IF_UPDATE] Found Update temp folder, moving files back.."); //if updated via store
        copy_dir("/user/update_tmp/covers", "/user/app/ITEM00001/covers");
		copy_dir("/user/update_tmp/custom_app_names", "/user/app/ITEM00001/custom_app_names");
		copyFile("/user/update_tmp/settings.ini", "/user/app/ITEM00001/settings.ini", false);
		rmtree("/user/update_tmp");
        log_debug("[IF_UPDATE] Moved files back, removing temp folder..");
	}
	else
       log_info("No Update temp folder found");
		    
    if(!is_connected_app){
        log_debug("[STARTUP][UPDATE] Daemon Not connected to app, exiting update mode ... ");
        sceMsgDialogTerminate();
        return;
    }
    if(if_exists(APP_PATH("../if_update.pkg"))){
        log_debug("[STARTUP][UPDATE][HDD] if_update.pkg exists, starting update ...");
        found = true;
    }
    else if(usbpath() != -1 && if_exists(fmt::format("/mnt/usb{0:d}/if_update.pkg", usbpath()).c_str())){
        log_debug("[STARTUP][UPDATE][USB] if_update.pkg exists, starting update ...");
        copyFile(fmt::format("/mnt/usb{0:d}/if_update.pkg", usbpath()), APP_PATH("../if_update.pkg"), false);
        found = true;
    }
    else {
        log_debug("[STARTUP][UPDATE] if_update.pkg does not exist, exiting update mode ...");
        sceMsgDialogTerminate();
        return;
    }
    if(found){
        log_debug("[STARTUP][UPDATE] Update Found, Sending IPC Command for Update Installing ...");
        sprintf(&ipc_msg[1], "%i", getpid());
        log_info("[STARTUP] Sending PID: %s", &ipc_msg[1]);
        int error = IPCSendCommand(INSTALL_IF_UPDATE, (uint8_t*)&ipc_msg[0]);
        if (error != NO_ERROR) {
            log_error("[STARTUP][UPDATE] IPC INSTALL UPDATE ERROR: %i", error);
            sceMsgDialogTerminate();
            return;
        }
    }
    else{
        log_debug("[STARTUP][UPDATE] if_update.pkg does not exist, exiting update mode ...");
        sceMsgDialogTerminate();
        return;
    }

    raise(SIGQUIT);
}
/*============================================================================================*/

void notify(const char* message)
{
    
    log_info(message);
    sceKernelLoadStartModule("/system/common/lib/libSceSysUtil.sprx", 0, NULL, 0, 0, 0);
    sceSysUtilSendSystemNotificationWithText(222, message);
}

// https://github.com/OSM-Made/PS4-Notify/blob/c6d259bc5bd4aa519f5b0ce4f5e27ef7cb01ffdd/Notify.cpp
void notifywithicon(const char* IconURI, const char* MessageFMT, ...)
{
  NotifyBuffer Buffer;
  
	va_list args;
	va_start(args, MessageFMT);
	vsprintf(Buffer.Message, MessageFMT, args);
	va_end(args);
  
  Buffer.Type = NotifyType::NotificationRequest;
	Buffer.unk3 = 0; 
	Buffer.UseIconImageUri = 1;
	Buffer.TargetId = -1;
	strcpy(Buffer.Uri, IconURI);
  sceKernelSendNotificationRequest(0, (char*)&Buffer, sizeof(Buffer), 0);
}

void msgok(enum MSG_DIALOG level, std::string in)
{
    sceMsgDialogTerminate();
    std::string out;

    switch (level)
    {
    case NORMAL:
        out = in;
        break;
    case FATAL:

        log_fatal(in.c_str());
        out =  fmt::format("{0:} {1:}\n\n {2:}", getLangSTR(FATAL_ERROR), in, getLangSTR(PRESS_OK_CLOSE));
        break;
    case WARNING:
        log_warn( in.c_str());
        out =  fmt::format("{0:} {1:}", getLangSTR(WARNING2), in);
        break;
    }

    sceMsgDialogInitialize();
    OrbisMsgDialogParam param;
    OrbisMsgDialogParamInitialize(&param);
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

    OrbisMsgDialogUserMessageParam userMsgParam;
    memset(&userMsgParam, 0, sizeof(userMsgParam));
    userMsgParam.msg        = out.c_str();
    userMsgParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
    param.userMsgParam      = &userMsgParam;

    if (sceMsgDialogOpen(&param) < 0) log_fatal( "MsgD failed");
    
    OrbisCommonDialogStatus stat;

    while (1) {
        stat = sceMsgDialogUpdateStatus();
        if (stat == ORBIS_COMMON_DIALOG_STATUS_FINISHED)
        {
            OrbisMsgDialogResult result;
            memset(&result, 0, sizeof(result));

            if (0 > sceMsgDialogGetResult(&result)) log_fatal( "MsgD failed"); 

            sceMsgDialogTerminate();
            break;
        }
    }

    if (level == FATAL) {
        log_fatal(in.c_str());
        raise(SIGQUIT);
    }

    return;
}

void loadmsg(std::string in)
{

    OrbisMsgDialogButtonsParam buttonsParam;
    OrbisMsgDialogUserMessageParam messageParam;
    OrbisMsgDialogParam dialogParam;

    sceMsgDialogInitialize();
    OrbisMsgDialogParamInitialize(&dialogParam);

    memset(&buttonsParam, 0x00, sizeof(buttonsParam));
    memset(&messageParam, 0x00, sizeof(messageParam));

    dialogParam.userMsgParam = &messageParam;
    dialogParam.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

    messageParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_WAIT;
    messageParam.msg        = in.c_str();

    sceMsgDialogOpen(&dialogParam);

    return;
}

bool install_IF_Theme(std::string theme_path){

    theme t;
    int ret = -9;
    int h, w;
    std::string tmp;
    fmt::print("[THEME][START] Installing Theme: {}", theme_path);

    if(!if_exists(APP_PATH("theme"))){
        log_error("[THEME] No Theme already exists, Making folder");
        mkdir(APP_PATH("theme"), 0777);
    }
    else{
        log_debug("[THEME] Theme already exists, Removing");
        rmtree(APP_PATH("theme"));
        log_debug("[THEME] Theme Removed");
    }
    log_debug("[THEME] Extracting Theme ...");
    if(!extract_zip(theme_path.c_str(), APP_PATH("theme"))){
        log_error("[THEME] Failed to extract Theme");
        goto err;
    }
    log_debug("[THEME] Extracting Theme Done");
    log_debug("[THEME] Getting Theme Info");

    t.is_image = false;
    t.using_sb = false;
    get->setting_strings[FNT_PATH] = "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF";

    if(!if_exists(APP_PATH("theme/theme.ini")) || (ret = ini_parse(APP_PATH("theme/theme.ini"), theme_info, &t)) < 0){
        log_error("[THEME][ERROR] Failed to Fetch Theme Info, ERROR %i", ret);
        goto err;
    }
    /*============================ QA Theme CHECKS ================================================*/
    if(t.version.empty() || t.name.empty() || t.author.empty() || t.date.empty()){
        log_error("[THEME][ERROR] Theme Info is missing");
        goto err;
    }
    if(t.is_image && (!if_exists(APP_PATH("theme/background.png")) || !is_png_vaild(APP_PATH("theme/background.png"), &h, &w))){
        log_error("[THEME][ERROR] No vaild Theme Image but the INI reports its using one");
        goto err;
    }
    if(t.has_font  && !if_exists(APP_PATH("theme/font.ttf")) ){
        log_error("[THEME][ERROR] No vaild FONT but the INI reports its using one");
        goto err;
    }
    for(int i = 0; i < sizeof required_theme_files / sizeof required_theme_files[0]; i++){

        tmp = fmt::format("{}{}", APP_PATH(""), required_theme_files[i]);

        if(!if_exists(tmp.c_str()) || !is_png_vaild(tmp.c_str(), &h, &w)){
           log_error("[THEME][ERROR] Error Loading file %s which is required for themes, maybe its invaild/corrupt?", &tmp[0]);
           goto err;
        }
    }
    if(t.using_sb  && !if_exists(APP_PATH("theme/shader.bin"))){
        log_error("[THEME][ERROR] No Shader Binary but the INI reports its using one");
        goto err;
    }
    else if(t.using_sb){
        int programID = -1;
        const char* data = (const char*)orbisFileGetFileContent(APP_PATH("theme/shader.bin")); 
        if(data == NULL){
            log_debug("[THEME][ERROR] can't load shader.bin");
            goto err;
        }
        else{
           programID = BuildProgram(p_vs1, data, p_vs1_length, _orbisFile_lastopenFile_size);
           if(programID == -1){
               log_debug("[THEME][ERROR] Shader Testing Failed!!!");
               free((void*)data);
               goto err;
           }
           else
              log_debug("[THEME][INFO] Shader Built... programID: %i", programID);
           // NO leaks pls
           glDeleteProgram(programID);
           free((void*)data);

        }
    }

    log_debug("[THEME] ======== Theme Info =======");

    fmt::print("[THEME] Name: {}", t.name);
    fmt::print("[THEME] Version: {}", t.version);
    fmt::print("[THEME] Date: {}", t.date);
    fmt::print("[THEME] Author: {}", t.author);

    log_debug("[THEME] Image: %s", t.is_image ? "Yes" : "No");
    log_debug("[THEME] Font: %s", t.has_font ? "Yes" : "No");
    log_debug("[THEME] Shader: %s", t.using_sb ? "Yes" : "No");
    log_debug("[THEME] ========  End of Theme Info =======");
    log_debug("[THEME] Freeing Theme Info ...");

    get->setting_bools[has_image] = t.is_image;
    get->setting_bools[using_sb] = t.using_sb;
    get->setting_bools[using_theme] = true;

    if(!SaveOptions(get)){
        log_error("[THEME] Failed to save Theme Options");
        goto err;
    }
    
    zip_create(APP_PATH("Reset Theme to Default.zip"), &if_d_zip[0], 200);
    
    if(usbpath() != -1){
        log_debug("[THEME] USB Path is set, updating USB Theme");
        tmp = fmt::format("/mnt/usb{}/Reset Theme to Default.zip", usbpath());
        zip_create(tmp.c_str(), &if_d_zip[0], 200);

    }

    fmt::print("[THEME] Theme Installed: {}", t.name);
    sceMsgDialogTerminate();

    return true;

err:
   log_error("[THEME][ERROR] Freeing Theme Info ...");
   log_error("[THEME][ERROR] Deleting Theme Dir ...");
   rmtree(APP_PATH("theme"));
   log_error("[THEME][ERROR] Theme Dir Deleted");
   log_error("[THEME][ERROR] Resetting IF Settings  ...");
   get->setting_strings[FNT_PATH] = "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF";
   get->setting_bools[using_theme] = false;
   if(!SaveOptions(get))
       log_error("[THEME][ERROR] Failed to save Theme Options");

   log_error("[THEME][ERROR] Complete.. returning....");
   sceMsgDialogTerminate();
   return false;
}

uint32_t Launch_App(std::string TITLE_ID, bool silent, int index) {

    uint32_t sys_res = -1;
    u32 userId = -1;

    int libcmi = sceKernelLoadStartModule("/system/common/lib/libSceSystemService.sprx", 0, NULL, 0, 0, 0);
    if (libcmi > 0)
    {
        fmt::print("Starting action Launch_App({0:}, {1:d})", TITLE_ID, silent);
        fmt::print("sceUserServiceGetForegroundUser {0:#x}", sceUserServiceGetForegroundUser(&userId));
        fmt::print("Launching App for User: {0:#x}", userId);

        LncAppParam param;
        param.sz = sizeof(LncAppParam);
        param.user_id = userId;
        param.app_opt = 0;
        param.crash_report = 0;
        param.check_flag = SkipSystemUpdateCheck;

        sys_res = sceLncUtilLaunchApp(TITLE_ID.c_str(), NULL, &param);
        if (IS_ERROR(sys_res))
        {
            if (!silent) {
                fmt::print("[ERROR] Switch {0:#x}", sys_res);
                switch (sys_res) {
                case SCE_LNC_ERROR_APP_NOT_FOUND: {
                    //msgok(WARNING, getLangSTR(APP_NOT_FOUND));
                    if(Confirmation_Msg(fmt::format("{0:}\n\n{1:}\n{2:}", getLangSTR(IF_LAUNCH_TROUBLESHOOTER), getLangSTR(LAUNCH_FAILED_BC_NOT_FOUND), getLangSTR(CONFIRMATION_2))) == YES){
                       if(Fix_Game_In_DB(all_apps, index, all_apps[index].is_fpkg))
                          ani_notify(NOTIFI_GAME, getLangSTR(GAME_INSERT_SUCCESSFUL), getLangSTR(REPAIR_ATTEMPTED1));
                        else
                          msgok(WARNING, getLangSTR(GAME_INSERT_FAILED));
                    }
                    break;
                }
                case SCE_LNC_UTIL_ERROR_ALREADY_RUNNING: {
                    msgok(WARNING, getLangSTR(APP_OPENED));
                    break;
                }
                case SCE_LNC_UTIL_ERROR_APPHOME_EBOOTBIN_NOT_FOUND: {
                    msgok(WARNING, getLangSTR(MISSING_EBOOT));
                    break;
                }
                case SCE_LNC_UTIL_ERROR_APPHOME_PARAMSFO_NOT_FOUND: {
                    msgok(WARNING, getLangSTR(MISSING_SFO));
                    break;
                }
                case SCE_LNC_UTIL_ERROR_NO_SFOKEY_IN_APP_INFO: {
                    //msgok(WARNING, getLangSTR(CORRUPT_SFO));
                    if(Confirmation_Msg(fmt::format("{0:}\n\n{1:}\n{2:}", getLangSTR(IF_LAUNCH_TROUBLESHOOTER), getLangSTR(FIX_BROKEN_SFO_DB), getLangSTR(CONFIRMATION_3))) == YES){
                       if(Fix_Game_In_DB(all_apps, index, all_apps[index].is_fpkg))
                          ani_notify(NOTIFI_GAME, getLangSTR(REPAIR_ATTEMPTED), getLangSTR(REPAIR_ATTEMPTED1));
                       else
                          msgok(WARNING, getLangSTR(GAME_INSERT_FAILED));
                    }
                    break;
                }
                case SCE_LNC_UTIL_ERROR_ALREADY_RUNNING_KILL_NEEDED: {
                    log_debug("ALREADY RUNNING KILL NEEDED");
                    break;
                }
                case SCE_LNC_UTIL_ERROR_ALREADY_RUNNING_SUSPEND_NEEDED: {
                    log_debug("ALREADY RUNNING SUSPEND NEEDED");
                    break;
                }
                case SCE_LNC_UTIL_ERROR_SETUP_FS_SANDBOX: {
                    msgok(WARNING, getLangSTR(APP_UNL));
                    break;
                }
                case SCE_LNC_UTIL_ERROR_INVALID_TITLE_ID: {
                    msgok(WARNING, getLangSTR(ID_NOT_VAILD));
                    break;
                }

                default: {
                    msgok(WARNING, fmt::format("{0:}: {1:#x}", getLangSTR(LAUNCH_ERROR),sys_res));
                    break;
                }
                }
            }
        }

        fmt::print("launch ret {0:#x}", sys_res);
    }
    else {
        if(!silent)
            msgok(WARNING, fmt::format("{0:}: {1:#x}", getLangSTR(LAUNCH_ERROR),libcmi));
    }

    return sys_res;
}

bool is_sfo(const char* input_file_name) {
    FILE* file = fopen(input_file_name, "rb");
    if (file == NULL) return false;

    // Get SFO header offset
    uint32_t magic;
    fread(&magic, 4, 1, file);
     if (magic == 1179865088) { // Param.sfo file
        rewind(file);
     }
     else
	 {
		log_error("[SFO] Not a param.sfo file. %s", input_file_name);
		fclose(file);
		return false;
	 }
	fclose(file);
	return true;
}

bool is_fpkg(std::string pkg_path) {

    PKG_HDR hdr;
    // Open path
    int pkg = open(pkg_path.c_str(), 0, 0);
    if (pkg < 0){  
        fmt::print("[ERROR] Error opening file: {}", pkg_path);
        return false;
    }
    
    lseek(pkg, 0, SEEK_SET);
    //first byte
    read(pkg, &hdr, sizeof(PKG_HDR));
    close(pkg);

    //fmt::print("pkg_path: {0:} | hdr.magic: {1:x}", pkg_path, hdr.magic);

    return (hdr.flags & FPKG) == 0;
}

bool dlc_pkg_details(const char* src_dest, DLC_PKG_DETAILS &details) {

    // Read PKG header
    struct pkg_header hdr;
    //char title[256];
    static const uint8_t pkg_magic[] = { '\x7F', 'C', 'N', 'T' };
    // Open path
    int pkg = sceKernelOpen(src_dest, O_RDONLY, 0);
    if (pkg < 0){
        log_error("Error opening file: %s, err: %s", src_dest, strerror(errno));
        return false;
    }

    sceKernelLseek(pkg, 0, SEEK_SET);
    sceKernelRead(pkg, &hdr, sizeof(struct pkg_header));

    if (memcmp(hdr.magic, pkg_magic, sizeof(pkg_magic)) != 0) {
        log_error("PKG Format is wrong");
        sceKernelClose(pkg);
        return false;
    }

    sceKernelClose(pkg);
    details.contentid = std::string(hdr.content_id);
    if(!extract_sfo_from_pkg(src_dest, "/user/app/ITEM00001/tmp_param.sfo")){
        log_error("Could not extract param.sfo");
        return false;
    }
    //TODO make more effecient 
    std::vector<uint8_t> sfo_data = readFile("/user/app/ITEM00001/tmp_param.sfo");
    if(sfo_data.empty()){ 
       log_error("Could not read param.sfo");
       return false;
    }

    SfoReader sfo(sfo_data);
    
    details.name = sanitizeString(sfo.GetValueFor<std::string>("TITLE"));
    details.version = sfo.GetValueFor<std::string>("VERSION");
    details.esd.insert(std::pair<std::string, std::string>("IRO_TAG", sfo.GetValueFor<std::string>("IRO_TAG")));

    for (int i = 0; i <= 30; i++) {
         std::string key = fmt::format("TITLE_{0:}{1:d}", (i < 10) ? "0" : "", i);
         details.esd.insert(std::pair<std::string, std::string>(key, sanitizeString(sfo.GetValueFor<std::string>(key))));
    }
    //log_info("[DLC] Content ID: %s", details.contentid.c_str());
    //log_info("[DLC] Version: %s", details.version.c_str());
    log_info("[DLC] Title: %s", details.name.c_str());

    return true;
}

void rebuild_db(){

    if (get->sort_cat == NO_CATEGORY_FILTER || 
    (get->sort_cat != NO_CATEGORY_FILTER && Confirmation_Msg(getLangSTR(CAT_REBUILD_WARNING)) == YES))
    {

        progstart((char *)getLangSTR(REBUILDING_FOR_FPKG).c_str());
        int failures = 0;
        for (int i = 1; i < all_apps[0].HDD_count - 1; i++)
        {
            bool is_if = (all_apps[i].id == "ITEM00001");
            if (!all_apps[i].is_fpkg || is_if)
            { // if its not an fpkg or Itemzflow, skip
                fmt::print("Skipping {}: {}", is_if ? "IF" : "Retail Game", all_apps[i].name);
                continue;
            }

            if (i >= all_apps[0].HDD_count){
                log_info("Skipping external HDD app: %s ???", all_apps[i].name.c_str());
                continue;
            }

            int prog = (uint32_t)(((float)i / all_apps[0].HDD_count) * 100.f);
            ProgSetMessagewText(prog, fmt::format("{0:}\n{1:}\n{2:}\n\n {3:} {4:d}\n{5:} {6:d}", getLangSTR(REBUILDING_FOR_FPKG), all_apps[i].name, all_apps[i].id, getLangSTR(CURRENT_APP), i, getLangSTR(TOTAL_NUM_OF_APPS), all_apps[0].HDD_count).c_str());
            log_info("App Info: %s | %i", all_apps[i].name.c_str(), i);
            // function also replaces special chars for SQL smt
            if (Fix_Game_In_DB(all_apps, i, all_apps[i].is_ext_hdd))
                log_info("Game Fixed: %s", all_apps[i].name.c_str());
            else
            {
                log_error("Game NOT Fixed: %s", all_apps[i].name.c_str());
                failures++;
            }
        }
        sceMsgDialogTerminate();
        loadmsg((char *)getLangSTR(REBUILDING_ADDCONT).c_str());
        addcont_dlc_rebuild("/system_data/priv/mms/addcont.db", false);
        //addcont_dlc_rebuild("/system_data/priv/mms/addcont.db", true);
        msgok(NORMAL, fmt::format("{0:}: {1:d}\n{2:}: {3:d}", getLangSTR(NUMB_OF_FIXED_APPS), all_apps[0].HDD_count-failures, getLangSTR(NOT_FIXED), failures));
    }    
}

void rebuild_dlc_db(){
    loadmsg((char *)getLangSTR(REBUILDING_ADDCONT).c_str());
    addcont_dlc_rebuild("/system_data/priv/mms/addcont.db", true);
    if(addcont_dlc_rebuild("/system_data/priv/mms/addcont.db", false))
       ani_notify(NOTIFI_SUCCESS, getLangSTR(ADDCONT_REBUILT), "");
    else
       ani_notify(NOTIFI_WARNING, getLangSTR(ADDCONT_FAILED), "");

    sceMsgDialogTerminate();

}

void print_Apps_Array(std::vector<item_t> &b)
{
    for (int i = 0; i <= b.at(0).token_c; i++)
    {
        fmt::print("b[{0:d}].token_d[0].off ptr: {1:}", i, b.at(i).id);
        fmt::print("b[{0:d}].token_d[1].off ptr: {1:}", i, b.at(i).name);
    }
}
void delete_apps_array(std::vector<item_t> &apps_arr)
{
    if(apps_arr.empty())
        return;

    for (int i = 1; i <= apps_arr.at(0).token_c; i++)
    {
        if (apps_arr.at(i).texture > GL_NULL){
            glDeleteTextures(1, &apps_arr.at(i).texture);
            apps_arr.at(i).texture = GL_NULL;
        }
    }

    apps_arr.clear();
}

std::ifstream::pos_type file_size(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg(); 
}


void refresh_apps_for_cf(Sort_Multi_Sel op, Sort_Category cat )
{
    pthread_mutex_lock(&disc_lock);
    int before = all_apps[0].token_c;
    log_debug("Reloading Installed Apps before: %i", before);
    loadmsg(getLangSTR(RELOAD_LIST));
    // leak not so much
    // print_Apps_Array(all_apps, before);
    delete_apps_array(all_apps);

    index_items_from_dir(all_apps, APP_PATH("../"), "/mnt/ext0/user/app", cat);
    log_info("=== %i", all_apps[0].token_c);
    //std::vector<item_idx_t*> sort;
    //print_Apps_Array(all_apps);

    switch (op)
    {
    case TID_ALPHA_SORT:
    {
        std::sort(all_apps.begin() + 1, all_apps.end(), [](item_t a, item_t b)
        { 
            return a.id.size() < b.id.size(); 
        });
        break;
    }
    case TITLE_ALPHA_SORT:
    {
        std::sort(all_apps.begin() + 1, all_apps.end(), [](item_t a, item_t b)
        { 
            std::string tmp1 = a.name;
            std::string tmp2 = b.name;

            std::transform(tmp1.begin(), tmp1.end(), tmp1.begin(), ::tolower);
            std::transform(tmp2.begin(), tmp2.end(), tmp2.begin(), ::tolower);

            return tmp1.compare(tmp2) < 0;
       });
        break;
    }
    case NA_SORT:
    default:
        break;
    }

    realloc_app_retries();
    InitScene_4(1920, 1080);
    for (int i = 1; i < all_apps[0].token_c + 1; i++)
    {
        check_tex_for_reload(i);
        check_n_load_textures(i);
    }

    sceMsgDialogTerminate();
    log_debug("Done reloading # of App: %i, # of Apps added/removed: %i", all_apps[0].token_c, all_apps[0].token_c - before);
    is_disc_inserted = false;
    inserted_disc = -999;
    g_idx = 1;
    pthread_mutex_unlock(&disc_lock);
}


