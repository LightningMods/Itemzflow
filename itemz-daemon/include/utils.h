#pragma once
#include <ps4sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include "defines.h"
#include "log.h"

#define SYS_dynlib_get_info         593
#define SYS_dynlib_get_info_ex      608

#define SYS_PROC_ALLOC              1
#define SYS_PROC_FREE               2
#define SYS_PROC_PROTECT            3
#define SYS_PROC_VM_MAP             4
#define SYS_PROC_INSTALL            5
#define SYS_PROC_CALL               6
#define SYS_PROC_ELF                7
#define SYS_PROC_INFO               8
#define SYS_PROC_THRINFO            9

#define SYS_CONSOLE_CMD_REBOOT      1
#define SYS_CONSOLE_CMD_PRINT       2
#define SYS_CONSOLE_CMD_JAILBREAK   3
#define SYS_CONSOLE_CMD_ISLOADED    6

#define DIM(x)  (sizeof(x)/sizeof(*(x)))
#define KB(x)   ((size_t) (x) << 10)
#define MB(x)   ((size_t) (x) << 20)

#define STAT_DB "/data/itemzflow_daemon/stats.db"
#define DAEMON_LOG_PS4 "/data/itemzflow_daemon/daemon.log"
#define DAEMON_LOG_USB "/mnt/usb0/itemzflow/daemon.log"
bool touch_file(char* destfile);
enum IPC_Errors
{
    INVALID = -1,
    NO_ERROR = 0,
    OPERATION_FAILED = 1
};

enum cmd
{
    SHUTDOWN_DAEMON = 0,
    CONNECTION_TEST = 1,
    ENABLE_HOME_REDIRECT = 2,
    DISABLE_HOME_REDIRECT = 3,
    GAME_GET_MINS = 4,
    GAME_GET_START_TIME = 5,
    CRITICAL_SUSPEND = 6,
    INSTALL_IF_UPDATE = 7,
    RESTART_FTP_SERVICE = 8,
    DEAMON_UPDATE = 100
};


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


struct clientArgs {
    char* ip;
    int socket;
    int cl_nmb;
};

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

typedef struct OrbisUserServiceLoginUserIdList {
    int32_t userId[4];
}  OrbisUserServiceLoginUserIdList;

typedef struct OrbisUserServiceInitializeParams {
    int32_t priority;
} OrbisUserServiceInitializeParams;

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

bool rejail();
bool jailbreak(const char* prx_path);
bool full_init();
bool isRestMode();
bool IsOn();
void notify(char* message);

extern int DaemonSocket;

#define networkSendMessage(socket, format, ...)\
do {\
	char msgBuffer[512];\
	int msgSize = sprintf(msgBuffer, format, ##__VA_ARGS__);\
	sceNetSend(socket, msgBuffer, msgSize, 0);\
} while(0)

void handleIPC(struct clientArgs* client, uint8_t* buffer, uint32_t length);
void *IPC_loop(void* args);
void* ipc_client(void* args);
bool check_update_from_url(const char* url);
bool create_stat_db(const char* path);
bool open_stat_db(const char* path);
bool update_game_time_played(const char* tid);
int sceUserServiceGetForegroundUser(int *userid);
int32_t sceSystemServiceHideSplashScreen();
uint32_t sceLncUtilLaunchApp(const char* tid, int opt, LncAppParam* param);
void  print_memory();
int sceSystemServiceLoadExec(const char* eboot, int idc);
const char* Language_GetName(int m_Code);
int sceSystemServiceGetAppIdOfMiniApp();
int sceSystemServiceGetAppIdOfBigApp();
int sceLncUtilGetAppId(const char* TID);
int sceLncUtilGetAppTitleId(int appid, char* tid_out);
int scePadSetProcessPrivilege(int privilege);  // 0 = no privilege, 1 = privilege
int sceLncUtilInitialize();
bool if_exists(const char* path);
int sceSystemServiceRegisterDaemon();
void  loadModulesVanilla();
int backtrace(void** buffer, int size);
int pthread_getthreadid_np();
int get_game_time_played(const char* tid);
time_t get_game_start_date(const char* tid);
void DumpHex(const void* data, size_t size);
uint32_t pkginstall(const char *path, bool is_if_update);

#define SWAP32(x) \
	((uint32_t)( \
		(((uint32_t)(x) & UINT32_C(0x000000FF)) << 24) | \
		(((uint32_t)(x) & UINT32_C(0x0000FF00)) <<  8) | \
		(((uint32_t)(x) & UINT32_C(0x00FF0000)) >>  8) | \
		(((uint32_t)(x) & UINT32_C(0xFF000000)) >> 24) \
	))

#define BE32(x) SWAP32(x)

struct _SceBgftDownloadRegisterErrorInfo {
    /* TODO */
    uint8_t buf[0x100];
};

enum bgft_task_option_t {
    BGFT_TASK_OPTION_NONE = 0x0,
    BGFT_TASK_OPTION_DELETE_AFTER_UPLOAD = 0x1,
    BGFT_TASK_OPTION_INVISIBLE = 0x2,
    BGFT_TASK_OPTION_ENABLE_PLAYGO = 0x4,
    BGFT_TASK_OPTION_FORCE_UPDATE = 0x8,
    BGFT_TASK_OPTION_REMOTE = 0x10,
    BGFT_TASK_OPTION_COPY_CRASH_REPORT_FILES = 0x20,
    BGFT_TASK_OPTION_DISABLE_INSERT_POPUP = 0x40,
    BGFT_TASK_OPTION_DISABLE_CDN_QUERY_PARAM = 0x10000,
};

struct bgft_download_param {
    int user_id;
    int entitlement_type;
    const char* id;
    const char* content_url;
    const char* content_ex_url;
    const char* content_name;
    const char* icon_path;
    const char* sku_id;
    enum bgft_task_option_t option;
    const char* playgo_scenario_id;
    const char* release_date;
    const char* package_type;
    const char* package_sub_type;
    unsigned long package_size;
};



struct bgft_download_param_ex {
    struct bgft_download_param param;
    unsigned int slot;
};



struct bgft_task_progress_internal {
    unsigned int bits;
    int error_result;
    unsigned long length;
    unsigned long transferred;
    unsigned long length_total;
    unsigned long transferred_total;
    unsigned int num_index;
    unsigned int num_total;
    unsigned int rest_sec;
    unsigned int rest_sec_total;
    int preparing_percent;
    int local_copy_percent;
};


struct bgft_init_params {
    void  *heap;
    size_t heapSize;
};

struct bgft_download_task_progress_info {
    unsigned int bits;
    int error_result;
    unsigned long length;
    unsigned long transferred;
    unsigned long length_total;
    unsigned long transferred_total;
    unsigned int num_index;
    unsigned int num_total;
    unsigned int rest_sec;
    unsigned int rest_sec_total;
    int preparing_percent;
    int local_copy_percent;
};

struct _SceBgftTaskProgress {
    unsigned int bits;
    int error_result;
    unsigned long length;
    unsigned long transferred;
    unsigned long lengthTotal;
    unsigned long transferredTotal;
    unsigned int numIndex;
    unsigned int numTotal;
    unsigned int restSec;
    unsigned int restSecTotal;
    int preparingPercent;
    int localCopyPercent;
};

typedef struct _SceBgftTaskProgress SceBgftTaskProgress;
typedef int SceBgftTaskId;
#define SCE_BGFT_INVALID_TASK_ID (-1)

bool app_inst_util_is_exists(const char* title_id, bool* exists);

#if defined(__cplusplus)
extern "C" {
#endif
int sceBgftServiceDownloadStartTask(SceBgftTaskId taskId);
int sceBgftServiceDownloadStartTaskAll(void);
int sceBgftServiceDownloadPauseTask(SceBgftTaskId taskId);
int sceBgftServiceDownloadPauseTaskAll(void);
int sceBgftServiceDownloadResumeTask(SceBgftTaskId taskId);
int sceBgftServiceDownloadResumeTaskAll(void);
int sceBgftServiceDownloadStopTask(SceBgftTaskId taskId);
int sceBgftServiceDownloadStopTaskAll(void);
int sceAppInstUtilTerminate();
int sceBgftServiceDownloadGetProgress(SceBgftTaskId taskId, SceBgftTaskProgress* progress);


int sceBgftFinalize(void);

bool app_inst_util_init(void);
int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppInstallPkg(const char* file_path, void* reserved);
int sceAppInstUtilGetTitleIdFromPkg(const char* pkg_path, char* title_id, int* is_app);
int sceAppInstUtilCheckAppSystemVer(const char* title_id, uint64_t buf, uint64_t bufs);
int sceAppInstUtilAppPrepareOverwritePkg(const char* pkg_path);
int sceAppInstUtilGetPrimaryAppSlot(const char* title_id, int* slot);
int sceAppInstUtilAppUnInstall(const char* title_id);
int sceAppInstUtilAppGetSize(const char* title_id, uint64_t* buf);
int sceBgftServiceInit(struct bgft_init_params*  params);
int sceBgftServiceIntDownloadRegisterTaskByStorageEx(struct bgft_download_param_ex* params, int* task_id);
int sceBgftServiceDownloadStartTask(int task_id);
int sceBgftServiceTerm(void);
#if defined(__cplusplus)
}  
#endif
bool copy_dir(const char* sourcedir,const char* destdir);
bool rmtree(const char path[]);
bool copyRegFile(const char* source, const char* dest);
void* initialize_userland_fuse();
long CalcAppsize(char *path);