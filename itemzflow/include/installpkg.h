#include <stdarg.h>
#include <stdlib.h> // calloc

#pragma once

#include "utils.h"

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

typedef struct install_args {
    char* title_id, *fname,* path; 
    bool is_thread = false, delete_pkg = true; 
    void* bgft_heap;
    int max_pkgs = 1, curr_pkg = 1, task_id = -1;
}install_args;

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

#define PKG_TITLE_ID_SIZE 0x9
#define PKG_SERVICE_ID_SIZE 0x13
#define PKG_CONTENT_ID_SIZE 0x24
#define PKG_LABEL_SIZE 0x10
#define PKG_DIGEST_SIZE 0x20
#define PKG_MINI_DIGEST_SIZE 0x14

#define SIZEOF_PKG_HEADER 0x2000

TYPE_BEGIN(struct pkg_header, SIZEOF_PKG_HEADER);
TYPE_FIELD(uint8_t magic[4], 0x00);
TYPE_FIELD(uint32_t entry_count, 0x10);
TYPE_FIELD(uint16_t sc_entry_count, 0x14);
TYPE_FIELD(uint32_t entry_table_offset, 0x18);
TYPE_FIELD(char content_id[PKG_CONTENT_ID_SIZE + 1], 0x40);
TYPE_FIELD(uint32_t content_type, 0x74);
TYPE_FIELD(uint32_t content_flags, 0x78);
#define PKG_CONTENT_FLAGS_FIRST_PATCH      0x00100000
#define PKG_CONTENT_FLAGS_PATCHGO          0x00200000
#define PKG_CONTENT_FLAGS_REMASTER         0x00400000
#define PKG_CONTENT_FLAGS_PS_CLOUD         0x00800000
#define PKG_CONTENT_FLAGS_GD_AC            0x02000000
#define PKG_CONTENT_FLAGS_NON_GAME         0x04000000
#define PKG_CONTENT_FLAGS_0x8000000        0x08000000 /* has data? */
#define PKG_CONTENT_FLAGS_SUBSEQUENT_PATCH 0x40000000
#define PKG_CONTENT_FLAGS_DELTA_PATCH      0x41000000
#define PKG_CONTENT_FLAGS_CUMULATIVE_PATCH 0x60000000
TYPE_FIELD(uint64_t package_size, 0x430);
TYPE_FIELD(uint8_t digest[PKG_DIGEST_SIZE], 0xFE0);
TYPE_END();
TYPE_CHECK_SIZE(struct pkg_header, SIZEOF_PKG_HEADER);




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
int sceBgftServiceIntDebugDownloadRegisterPkg(struct bgft_download_param_ex* params, int* task_id);
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
int sceBgftServiceDownloadRegisterTask(struct bgft_download_param *params, int *task);
int sceAppInstUtilAppGetSize(const char* title_id, uint64_t* buf);
int sceBgftServiceInit(struct bgft_init_params*  params);
int sceBgftServiceIntDownloadReregisterTaskPatch(SceBgftTaskId oldTaskId, SceBgftTaskId* newTaskId);
int sceBgftServiceIntDownloadRegisterTaskByStorageEx(struct bgft_download_param_ex* params, int* task_id);
int sceBgftServiceDownloadStartTask(int task_id);
int sceBgftServiceTerm(void);
#if defined(__cplusplus)
}  
#endif
uint32_t pkginstall(const char *fullpath, const char* filename, bool Show_install_prog, bool delete_pkg_after, int max_pkgs = 1, int curr_pkg = 1);
uint32_t installPatchPKG(const char *url, const char *title_id, const char* icon_path);
bool Install_Store();