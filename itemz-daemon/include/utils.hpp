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
bool touch_file(const char* destfile);

#define MAX_MESSAGE_SIZE    0x1000
#define MAX_STACK_FRAMES    60

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

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


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

bool rejail();
bool jailbreak(const char* prx_path);
bool full_init();
bool isRestMode();
bool IsOn();
void notify(const char* message);
void save_fuse_ip(const char* ip);
extern int DaemonSocket;

#define networkSendMessage(socket, format, ...)\
do {\
	char msgBuffer[512];\
	int msgSize = sprintf(msgBuffer, format, ##__VA_ARGS__);\
	sceNetSend(socket, msgBuffer, msgSize, 0);\
} while(0)

void  *IPC_loop(void* args);
bool check_update_from_url(const char* url);
bool create_stat_db(const char* path);
bool open_stat_db(const char* path);
bool update_game_time_played(const char* tid);
int get_game_time_played(const char* tid);
time_t get_game_start_date(const char* tid);
void DumpHex(const void* data, size_t size);
uint32_t pkginstall(const char *path, bool is_if_update);
struct fuse_actions {
    int argc;
    char argv[0x30];
    char path[0x60];
    char ip[0x100];
    bool is_debug_mode;
};
extern bool reboot_daemon;
int initialize_userland_fuse(const char* mp, const char* ip);

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
typedef struct LibcMallocManagedSize {
    uint16_t size;
    uint16_t version;
    uint32_t reserved1;
    size_t maxSystemSize;
    size_t currentSystemSize;
    size_t maxInuseSize;
    size_t currentInuseSize;
} LibcMallocManagedSize;


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
int sceSystemStateMgrGetCurrentState(void);
int sceSystemStateMgrEnterStandby(int a1);
int sceKernelOpenEventFlag(void* event, const char* name);
int sceKernelCloseEventFlag(void* event);
int sceKernelPollEventFlag(void* ef, uint64_t bitPattern, uint32_t waitMode, uint64_t *pResultPat);
bool sceSystemStateMgrIsStandbyModeEnabled(void);
int sceBgftServiceTerm(void);
int32_t sceSystemServiceHideSplashScreen();
int32_t sceSystemServiceParamGetInt(int32_t paramId, int32_t *value);
bool patch_sfo_string(const char* input_file_name, const char* key, const char* new_value );
uint32_t sceLncUtilLaunchApp(const char* tid, const char* argv[], LncAppParam* param);
void  print_memory();
int sceSystemServiceLoadExec(const char* eboot, const char* argv[]);
void drop_some_coverbox(void);
int sceUserServiceGetForegroundUser(int *userid);
int32_t sceSystemServiceHideSplashScreen();
const char* Language_GetName(int m_Code);
int sceSystemServiceGetAppIdOfMiniApp();
int sceSystemServiceGetAppIdOfBigApp();
int sceLncUtilGetAppId(const char* TID);
int sceLncUtilGetAppTitleId(int appid, char* tid_out);
int scePadSetProcessPrivilege(int privilege);  // 0 = no privilege, 1 = privilege
int sceLncUtilInitialize();
bool if_exists(const char* path);
int sceSystemServiceRegisterDaemon();
void* prx_func_loader(const char* prx_path, const char* symbol);
void  loadModulesVanilla();
int backtrace(void** buffer, int size);
int sceAppInstUtilAppGetInsertedDiscTitleId(const char* tid);
int scePadGetHandle(int uid, int idc, int idc_1);
int sceAppInstUtilTerminate();
int sceKernelBacktraceSelf(callframe_t frames[], size_t numBytesBuffer, uint32_t *pNumReturn, int mode);
int32_t sceKernelVirtualQuery(const void *, int, OrbisKernelVirtualQueryInfo *, size_t);
int sceKernelGetdents(int fd, char *buf, int nbytes);
int sceSysUtilSendSystemNotificationWithText(int messageType, const char* message);
int64_t sceKernelSendNotificationRequest(int64_t unk1, const char* Buffer, size_t size, int64_t unk2);
int fuse_fw_supported();
int sceKernelSetBesteffort(int besteffort, int priority);
int pthread_getthreadid_np();
struct tm *gmtime_s( const time_t *timer, struct tm *buf );
void* syscall1(
	uint64_t number,
	void* arg1
);
void* fuse_startup(void* arg);
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
int malloc_stats_fast(LibcMallocManagedSize *ManagedSize);
int sceKernelDebugOutText(int DBG_CHANNEL, const char *text);
uint32_t ps4_fw_version(void);
#if defined(__cplusplus)
}  
#endif
bool copy_dir(const char* sourcedir,const char* destdir);
bool rmtree(const char path[]);
bool copyRegFile(const char* source, const char* dest);
int fuse_nfs_main(struct fuse_actions args);
long CalcAppsize(const char *path);
void load_fuse_ip();
extern bool fuse_debug_flag;