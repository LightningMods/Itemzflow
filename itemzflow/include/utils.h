#pragma once


#include "defines.h"
#include "ini.h"
#include <sys/param.h>   // MIN
#if defined (__ORBIS__)
#include <dumper.h>
#endif
#include "GLES2_common.h"
#include <atomic>
#include <string>
#include <vector>
#include "log.h"
#include "sfo.hpp"
#include "lang.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "fmt/chrono.h"
#include <unordered_map>
#include <fstream>
#include <iostream>
#ifdef __ORBIS__
#include <user_mem.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#define DIM(x)  (sizeof(x)/sizeof(*(x)))
#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)
#define GB(x)   ((size_t) (x) << 30)
#define B2GB(x)   ((size_t) (x) >> 30)
#define B2MB(x)   ((size_t) (x) >> 20)


#define ORBIS_SYSMODULE_MESSAGE_DIALOG 0x00A4 // libSceMsgDialog.sprx
#define DUMPER_LOG "/user/app/ITEM00001/logs/if_dumper.log"
#define STORE_TID "NPXS39041"

/*
Dumper Path

Set as default XMB (home menu)

Sort Apps by

Dumper Option

FTP Auto Start

Save settings
*/
enum AppType
{
    Invalid = -1,
    Unknown,
    ShellUI,
    Daemon,
    CDLG,
    MiniApp,
    BigApp,
    ShellCore,
    ShellApp
};

typedef enum{
    MIN_STATUS_NA = -1,
    MIN_STATUS_RESET = -2,
    MIN_STATUS_SEEN = -3,
    MIN_STATUS_VAPP = -4,
} min_status;

typedef struct
{
    int appId;
    int launchRequestAppId;
    enum  AppType appType;
} AppStatus;

enum MSG_DIALOG {
    NORMAL,
    FATAL,
    WARNING
};

extern uint32_t daemon_appid;
enum IPC_Ret
{
    INVALID = -1,
    NO_ERROR = 0,
    OPERATION_FAILED = 1,
    FUSE_IP_ALREADY_SET = 3,
    FUSE_IP_NOT_SET = 4,
    FUSE_FW_NOT_SUPPORTED = 5,
    DEAMON_UPDATING = 100
};


enum IPC_Commands
{
        
    MACGUFFIN_CMD = 69,
    CMD_SHUTDOWN_DAEMON = 0,
    CONNECTION_TEST = 1,
    ENABLE_HOME_REDIRECT = 2,
    DISABLE_HOME_REDIRECT = 3,
    GAME_GET_MINS = 4,
    GAME_GET_START_TIME = 5,
    CRITICAL_SUSPEND = 6,
    INSTALL_IF_UPDATE = 7,
    RESTART_FTP_SERVICE = 8,
    FUSE_SET_SESSION_IP = 9,
    FUSE_SET_DEBUG_FLAG = 10,
    FUSE_START_W_PATH = 11,
    RESTART_FUSE_FS = 12,
    DEAMON_UPDATE = 100,
};




bool IPCOpenIfNotConnected();

typedef struct SceNetEtherAddr {
    uint8_t data[6];
} SceNetEtherAddr;

typedef union SceNetCtlInfo {
    uint32_t device;
    SceNetEtherAddr ether_addr;
    uint32_t mtu;
    uint32_t link;
    SceNetEtherAddr bssid;
    char ssid[33];
    uint32_t wifi_security;
    int32_t rssi_dbm;
    uint8_t rssi_percentage;
    uint8_t channel;
    uint32_t ip_config;
    char dhcp_hostname[256];
    char pppoe_auth_name[128];
    char ip_address[16];
    char netmask[16];
    char default_route[16];
    char primary_dns[16];
    char secondary_dns[16];
    uint32_t http_proxy_config;
    char http_proxy_server[256];
    uint16_t http_proxy_port;
} SceNetCtlInfo;



struct DLC_PKG_DETAILS {
    std::string version, name, contentid;
    std::unordered_map<std::string, std::string> esd; //extra sfo data
};

bool dlc_pkg_details(const char* src_dest, DLC_PKG_DETAILS &details);

typedef enum {
    VIS_READ = 0,
    VIS_WRTIE
} APP_DB_VIS;

#define DAEMON_INI "/user/app/ITEM00001/daemon.ini"
#define APP_DB "/system_data/priv/mms/app.db"
#define DAEMON_PATH "/system/vsh/app/ITEM00002"

bool AppDBVisiable(std::string tid, APP_DB_VIS opt, int write_value);

//#define assert(expr) if (!(expr)) msgok(FATAL, "Assertion Failed!");

#define YES 1
#define NO  2
int Confirmation_Msg(std::string msgr);
enum Flag
{
    Flag_None = 0,
    SkipLaunchCheck = 1,
    SkipResumeCheck = 1,
    SkipSystemUpdateCheck = 2,
    RebootPatchInstall = 4,
    VRMode = 8,
    NonVRMode = 16
};

typedef struct _LncAppParam
{
    uint32_t sz;
    uint32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    enum Flag check_flag;
}
LncAppParam;


#define BETA 0
//#define LOCALHOST_WINDOWS 1


extern int HDD_count;
enum CHECK_OPTS
{
    MD5_HASH,
    COUNT,
    DL_COUNTER,
#if BETA==1
    BETA_CHECK,  
#endif
    NUM_OF_STRING
};

#define SCE_SYSMODULE_INTERNAL_COMMON_DIALOG 0x80000018
#define SCE_SYSMODULE_INTERNAL_SYSUTIL 0x80000018

// indexed options
/*
typedef enum Dump_Options {
        BASE_GAME = 1,
        GAME_PATCH = 2,
        REMASTER = 3,
        THEME = 4,
        THEME_UNLOCK = 5,
        ADDITIONAL_CONTENT_DATA = 6,
        ADDITIONAL_CONTENT_NO_DATA = 7,
        TOTAL_OF_OPTS = 7
}  Dump_Options;
*/

extern Dump_Multi_Sels dump;

enum SORT_APPS_BY {
    SORT_NA = -1,
    SORT_RAND,
    SORT_TID,
    SORT_ALPHABET
};

// indexed options
#ifndef __ORBIS__
typedef enum Dump_Options {
    BASE_GAME = 1,
    GAME_PATCH = 2,
    REMASTER = 3,
    THEME = 4,
    THEME_UNLOCK = 5,
    ADDITIONAL_CONTENT_DATA = 6,
    ADDITIONAL_CONTENT_NO_DATA = 7,
    TOTAL_OF_OPTS = 7
}  Dump_Options;
#endif

typedef struct save_entry
{
    std::string name;
    std::string title_id;
    std::string path;
    std::string dir_name;
    std::string main_title;
    std::string sub_title;
    std::string detail;
    std::string mtime;
    std::string icon_path;

    int numb_of_saves = 0;
    bool is_loaded = false ;
    int ui_requested_save = -1;
    int ui_current_save = -1;
    int size = 0;
    uint32_t userid = 0;
    uint32_t blocks = 0;
    uint16_t flags;
    uint16_t type = 0;
    uint16_t userParam = 0;
} save_entry_t;

enum bool_settings
{
    HomeMenu_Redirection, 
    Daemon_on_start, 
    Show_install_prog, 
    Start_FTP, 
    FTP_on_Start, 
    Cover_message, 
    Show_Buttons, 
    un_apt,
    using_theme, 
    using_sb, 
    has_image,
    cover_message,
    INTERNAL_UPDATE,
    NUMB_OF_BOOLS

};

enum string_settings
{
    CDN_URL,
    DUMPER_PATH,
    MP3_PATH,
    INI_PATH,
    FNT_PATH,
    NUM_OF_STRINGS,
    SB_PATH,
    IMAGE_PATH,
    THEME_NAME,
    THEME_AUTHOR,
    THEME_VERSION,
    FUSE_PC_NFS_IP,
    NUMB_OF_STRINGS
};

// indexed options
typedef struct
{
    // setting strings
    std::unordered_map<int, std::string> setting_strings;
    std::unordered_map<int, bool> setting_bools;

    int lang, FTP_Port, ItemzCore_AppId, ItemzDaemon_AppId;
    Sort_Multi_Sel sort_by;
    Dump_Multi_Sels Dump_opt;
    Uninstall_Multi_Sel un_opt;
    Sort_Category sort_cat;
    
    // more options
} ItemzSettings;

#define IS_DISC_GAME 0x80990087
#define MIN_NUMBER_OF_IF_APPS 6


// the Settings
extern ItemzSettings set,
* get;
extern save_entry_t gm_save;
extern bool current_app_is_fpkg;
bool LoadOptions(ItemzSettings *set);
bool SaveOptions(ItemzSettings *set);
bool Keyboard(const char* Title, const char* initialTextBuffer, char* out_buffer, bool keypad = false);
IPC_Ret IPCMountFUSE(const char* ip, const char* path, bool debug_mode);


// sysctl
uint32_t ps4_fw_version(void);
bool is_goldhen_loaded();
bool copy_dir(const char* sourcedir, const char* destdir);
extern bool is_connected_app;
size_t CalcAppsize(const char* filename);
const char* cutoff(const char* str, int from, int to);
int getjson(int Pagenumb, char* cdn, bool legacy);
bool MD5_hash_compare(const char* file1, const char* hash);
bool copyFile(std::string source, std::string dest, bool show_progress);
void ProgSetMessagewText(int prog, const char* fmt, ...);
bool app_inst_util_uninstall_patch(const char* title_id, int* error);
bool app_inst_util_uninstall_game(const char *title_id, int *error);
int check_store_from_url(int page_number, char* cdn, enum CHECK_OPTS opt);
int check_download_counter(ItemzSettings* set, char* title_id);
bool rmtree(const char path[]);
void setup_store_assets(ItemzSettings* get);
int GetMessageOption();
int OpenConnection(const char* name);
bool IPCOpenConnection();
int IPCReceiveData(uint8_t* buffer, int32_t size);
int IPCSendData(uint8_t* buffer, int32_t size);
int IPCCloseConnection();
#define DAEMON_BUFF_MAX 100
void GetIPCMessageWithoutError(uint8_t* buf, uint32_t sz);
int mountfs(const char* device, const char* mountpoint, const char* fstype, const char* mode, uint64_t flags);
int check_free_space(const char* mountPoint);
unsigned int usbpath();
#define MAX_MESSAGE_SIZE    0x1000
#define MAX_STACK_FRAMES    60

/**
 * A callframe captures the stack and program pointer of a
 * frame in the call path.
 **/
typedef struct {
    char* sp;
    char* pc;
} callframe_t;

typedef struct {
    char* unk01;
    char* unk02;
    off_t offset;
    int unk04;
    int unk05;
    unsigned isFlexibleMemory : 1;
    unsigned isDirectMemory : 1;
    unsigned isStack : 1;
    unsigned isPooledMemory : 1;
    unsigned isCommitted : 1;
    char name[32];
} OrbisKernelVirtualQueryInfo;

typedef struct OrbisUserServiceLoginUserIdList {
	int32_t userId[4];
}  OrbisUserServiceLoginUserIdList;

#define ORBIS_USER_SERVICE_MAX_LOGIN_USERS      4   //Maximum number of users that can be logged in at once
#define ORBIS_USER_SERVICE_MAX_REGISTER_USERS   16  //Maximum number of users that can be registered in the system
#define ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH 16  //Maximum user name length

typedef struct OrbisUserServiceRegisteredUserIdList {
	int userId[ORBIS_USER_SERVICE_MAX_REGISTER_USERS];
} OrbisUserServiceRegisteredUserIdList;

typedef struct OrbisUserServiceInitializeParams {
	int32_t priority;
} OrbisUserServiceInitializeParams;


int sceUserServiceGetRegisteredUserIdList(OrbisUserServiceRegisteredUserIdList *);
int sceUserServiceGetUserName(int, char *, const size_t);

#define SCE_LNC_UTIL_ERROR_ALREADY_RUNNING 0x8094000c
#define USER_PATH_HDD   "/system_data/savedata/%08x/db/user/savedata.db"

extern std::string title;
extern std::string title_id;
extern std::string pic_path;
extern std::string icon_path;

typedef struct {
    uint32_t magic;
    uint32_t flags;
}PKG_HDR;

typedef enum {
    OFFICIAL_PKG = 0x83000001,
    PLAYROOM_PKG = 0x81000001,
    FPKG = 0x00000001
} PKG_Type;

void SIG_Handler(int sig_numb);

int ItemzLocalKillApp(uint32_t appid);
int Close_Game();
extern pthread_mutex_t notifcation_lock, disc_lock, usb_lock;
typedef struct
{
    unsigned int size;
    u32 userId;
} SceShellUIUtilLaunchByUriParam;

typedef enum
{
    FS_NOT_STARTED,
    FS_STARTED,
    FS_DONE,
    FS_CANCELLED,

} FILE_STATE;

typedef enum
{
    FS_NONE = 0,
    FS_MP3,
    FS_FONT,
    FS_PNG,
    FS_PKG,
    FS_ZIP,
    FS_FOLDER,
    FS_MP3_ONLY,

} FS_FILTER;

typedef struct
{
    layout_t *last_layout;
    int (*fs_func)(std::string file, std::string full_path);
    std::string selected_file, selected_full_path;
    view_t last_v;
    FILE_STATE status;
    FS_FILTER filter;
} fs_res;

extern int v_curr;
extern fs_res fs_ret;

typedef enum
{
    APP_NA_STATUS = -1,
    NO_APP_OPENED,
    RESUMABLE,
    OTHER_APP_OPEN,
} GameStatus;

extern GameStatus app_status;

bool is_sfo(const char* input_file_name);
int ItemzLaunchByUri(const char *uri);
int sceUserServiceGetForegroundUser(u32 *userid);
int32_t sceSystemServiceHideSplashScreen();
int32_t sceSystemServiceParamGetInt(int32_t paramId, int32_t *value);
bool patch_sfo_string(const char* input_file_name, const char* key, const char* new_value );
uint32_t sceLncUtilLaunchApp(const char* tid, const char* argv[], LncAppParam* param);
void  print_memory();
int sceSystemServiceLoadExec(const char* eboot, const char* argv[]);
void drop_some_coverbox(void);
int sceSystemServiceGetAppIdOfMiniApp();
int sceSystemServiceGetAppIdOfBigApp();
int sceLncUtilGetAppId(const char* TID);
int sceLncUtilGetAppTitleId(int appid, char* tid_out);
int sceAppInstUtilAppGetInsertedDiscTitleId(const char* tid);
int scePadGetHandle(int uid, int idc, int idc_1);
int sceAppInstUtilTerminate();

void* syscall1(
	uint64_t number,
	void* arg1
);

void* syscall2(
	uint64_t number,
	void* arg1,
	void* arg2
);

void* syscall3(
	uint64_t number,
	void* arg1,
	void* arg2,
	void* arg3
);

void* syscall4(
	uint64_t number,
	void* arg1,
	void* arg2,
	void* arg3,
	void* arg4
);

void* syscall5(
	uint64_t number,
	void* arg1,
	void* arg2,
	void* arg3,
	void* arg4,
	void* arg5
);
int64_t sys_dynlib_load_prx(char* prxPath, int* moduleID);
int64_t sys_dynlib_unload_prx(int64_t prxID);
int64_t sys_dynlib_dlsym(int64_t moduleHandle, const char* functionName, void* destFuncOffset);
// todo: sort out those proto
uint32_t sceKernelGetCpuTemperature(uint32_t* res);
int sceKernelAvailableFlexibleMemorySize(size_t* free_mem);
int sceAppInstUtilAppExists(const char* tid, int* flag);


const char* Language_GetName(int m_Code);
int progstart(char* format, ...);
int app_inst_util_get_size(const char* title_id);
bool app_inst_util_uninstall_ac(const char* content_id, const char* title_id, int* error);
bool app_inst_util_uninstall_game(const char *title_id, int *error);
bool touch_file(const char* destfile);
void dump_frame();
void trigger_dump_frame();

/*=================== IPC SHIT================================*/
void GetIPCMessageWithoutError(uint8_t* buf, uint32_t sz);
bool IPCOpenConnection();
int IPCReceiveData(uint8_t* buffer, int32_t size);
int IPCSendData(uint8_t* buffer, int32_t size);
int IPCCloseConnection();
IPC_Ret IPCSendCommand(enum IPC_Commands cmd, uint8_t* IPC_BUFFER);
/*===================++++++=================================*/
extern bool reboot_app;
bool IS_ERROR(uint32_t ret);
extern vertex_buffer_t *test_v; // pixelshader vbo
bool MD5_file_compare(const char* file1, const char* file2);
void Kill_BigApp(GameStatus &status);
void render_button(GLuint btn, float h, float w, float x, float y, float multi);
bool GameSave_Info(save_entry_t *item, int off);
int sceKernelOpenEventFlag(void* event, const char* name);
int sceKernelCloseEventFlag(void* event);
int sceKernelPollEventFlag(void* ef, uint64_t bitPattern, uint32_t waitMode, uint64_t *pResultPat);
int GetMountedUsbMassStorageIndex();
int sceSystemServiceLaunchApp(const char* titleId, const char* argv[], LncAppParam* param);
bool extract_zip(const char* zip_file, const char* dest_path);
bool power_settings(Power_control_sel sel);
void relaunch_timer_thread();
enum update_ret{
    IF_UPDATE_FOUND,
    IF_UPDATE_ERROR,
    IF_NO_UPDATE,
};
update_ret check_update_from_url(const char* tid);
void launch_update_thread();
bool shellui_for_lockdown(bool unpatch);
void mkdirs(const char* dir);
int sceShellCoreUtilRequestEjectDevice(const char* devnode);

unsigned char* orbisFileGetFileContent(const char* filename);
extern size_t _orbisFile_lastopenFile_size;
int sceKernelBacktraceSelf(callframe_t frames[], size_t numBytesBuffer, uint32_t *pNumReturn, int mode);
int32_t sceKernelVirtualQuery(const void *, int, OrbisKernelVirtualQueryInfo *, size_t);
int sceKernelGetdents(int fd, char *buf, int nbytes);
int sceSysUtilSendSystemNotificationWithText(int messageType, const char* message);
int64_t sceKernelSendNotificationRequest(int64_t unk1, const char* Buffer, size_t size, int64_t unk2);
bool addcont_dlc_rebuild(const char* db_path, bool ext_hdd);
bool Reactivate_external_content(bool is_addcont_open);
void Stop_Music();
void rebuild_dlc_db();
void rebuild_db();
bool extract_sfo_from_pkg(const char* pkg, const char* outdir);

enum NotifyType
{
	NotificationRequest = 0,
	SystemNotification = 1,
	SystemNotificationWithUserId = 2,
	SystemNotificationWithDeviceId = 3,
	SystemNotificationWithDeviceIdRelatedToUser = 4,
	SystemNotificationWithText = 5,
	SystemNotificationWithTextRelatedToUser = 6,
	SystemNotificationWithErrorCode = 7,
	SystemNotificationWithAppId = 8,
	SystemNotificationWithAppName = 9,
	SystemNotificationWithAppInfo = 9,
	SystemNotificationWithAppNameRelatedToUser = 10,
	SystemNotificationWithParams = 11,
	SendSystemNotificationWithUserName = 12,
	SystemNotificationWithUserNameInfo = 13,
	SendAddressingSystemNotification = 14,
	AddressingSystemNotificationWithDeviceId = 15,
	AddressingSystemNotificationWithUserName = 16,
	AddressingSystemNotificationWithUserId = 17,

	UNK_1 = 100,
	TrcCheckNotificationRequest = 101,
	NpDebugNotificationRequest = 102,
	UNK_2 = 102,
};

struct NotifyBuffer
{ //Naming may be incorrect.
	NotifyType Type;		//0x00 
	int ReqId;				//0x04
	int Priority;			//0x08
	int MsgId;				//0x0C
	int TargetId;			//0x10
	int UserId;				//0x14
	int unk1;				//0x18
	int unk2;				//0x1C
	int AppId;				//0x20
	int ErrorNum;			//0x24
	int unk3;				//0x28
	char UseIconImageUri; 	//0x2C
	char Message[1024]; 	//0x2D
	char Uri[1024]; 		//0x42D
	char unkstr[1024];		//0x82D
}; //Size = 0xC30

struct current_song_t {
    std::string song_title;
    std::string song_artist;
    int total_files = 0;
    int current_file = 0;
    bool is_playing = false;
};

extern current_song_t current_song; 

///int fuse_test();
#if defined(__cplusplus)
}
#endif
enum mp3_status_t{
    MP3_STATUS_STARTED,
    MP3_STATUS_PLAYING,
    MP3_STATUS_PAUSED,
    MP3_STATUS_ERROR
};
#ifndef __ORBIS__   
void sceMsgDialogTerminate(void);
#endif
extern std::vector<item_t> mp3_playlist;
double CalcFreeGigs(const char* path);
bool is_fpkg(std::string pkg_path);
bool patch_lnc_debug();
extern bool is_remote_va_launched;
bool do_update(std::string  url);
void SaveData_Operations(SAVE_Multi_Sel sv, std::string tid, std::string path);
struct sockaddr_in IPCAddress(uint16_t port);
bool getEntries(std::string path, std::vector<std::string> &cvEntries, FS_FILTER filter, bool for_fs_browser);
bool is_if_vapp(std::string tid);
void delete_apps_array(std::vector<item_t> &b);
void index_items_from_dir_v2(std::string dirpath, std::vector<item_t> &out_vec);
void refresh_apps_for_cf(Sort_Multi_Sel op, Sort_Category cat = NO_CATEGORY_FILTER);
bool change_game_name(std::string new_title, std::string tid, std::string sfo_path);
bool If_Save_Exists(const char *userPath, save_entry_t *item);
uint32_t Launch_App(std::string TITLE_ID, bool silent = false, int index = 0 );
bool StartFileBrowser(view_t vt, layout_t *l, int (*callback)(std::string fn, std::string fp), FS_FILTER filter);
mp3_status_t MP3_Player(std::string path);
bool install_IF_Theme(std::string theme_path);
void notify(const char* message);
// https://github.com/OSM-Made/PS4-Notify/blob/c6d259bc5bd4aa519f5b0ce4f5e27ef7cb01ffdd/Notify.cpp
void notifywithicon(const char* IconURI, const char* MessageFMT, ...);
void msgok(enum MSG_DIALOG level, std::string in);
void loadmsg(std::string in);
std::vector<uint8_t> readFile(std::string filename);
std::ifstream::pos_type file_size(const char* filename);
bool Fix_Game_In_DB(std::vector<item_t> &app_info, int index, bool is_ext_hdd);
int check_syscalls();
int mount_fuse(const char* device, const char* mountpoint);
extern int (*rejail_multi)(void);
void check_for_update_file();
std::string calculateSize(uint64_t size);
void* prx_func_loader(std::string prx_path, std::string symbol);
void try_func();
std::string check_from_url(std::string &url_);
bool init_curl();
bool Launch_Store_URI();
bool is_vapp(std::string tid);
std::string sanitizeString(const std::string& str);
void GLES2_Draw_idle_info(void);
bool get_ip_address(std::string &ip);
int get_folder_size(const char *file_path, u64 *size);
