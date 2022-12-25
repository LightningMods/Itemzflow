#pragma once

/*
    ES2_goodness, a GLES2 playground on EGL
    2019-2021 masterzorag & LM
    here follows the parts:
*/

// nfs or local stdio
//#define USE_NFS  (1)
#if defined (USE_NFS)
#include <orbisNfs.h>
#endif

#include "GLES2_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <utils.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include "log.h"

#define ITEMZFLOW_TID "ITEM00001"
#define GL_NULL 0
#define FM_ENTRIES (10) // max num we split bound_box Height, in px


#if defined (__ORBIS__)

#include <debugnet.h>
#include <orbisGl.h>
#include <orbislink.h>
#include <libkernel.h>  //sceKernelIccSetBuzzer
#include <libSceSysmodule.h>
#include <dialog.h>
#include <libkernel.h>
#include <orbis/libkernel.h>
#include <orbislink.h>
#include <sys/mman.h> 
#include "dialog.h"
#include <sys/_types.h>
#include <sys/fcntl.h> //O_CEAT
#include "md5.h"
#include "lang.h"
#include "dialog.h"

#define ITEMZ_CUSTOM_SFO "/user/app/ITEM00001/sfos"

#define SSL_HEAP_SIZE	(304 * 1024)
#define HTTP_HEAP_SIZE	MB(5)
#define NET_HEAP_SIZE   MB(5)

#define SCE_SYSMODULE_INTERNAL_SYS_CORE 0x80000004
#define SCE_SYSMODULE_INTERNAL_NETCTL 0x80000009
#define SCE_SYSMODULE_INTERNAL_HTTP 0x8000000A
#define SCE_SYSMODULE_INTERNAL_SSL 0x8000000B
#define SCE_SYSMODULE_INTERNAL_AUDIOOUT 0x80000001
#define SCE_SYSMODULE_INTERNAL_AUDIOIN 0x80000002
#define SCE_SYSMODULE_INTERNAL_NP_COMMON 0x8000000C
#define SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE 0x80000010
#define SCE_SYSMODULE_INTERNAL_USER_SERVICE 0x80000011
#define SCE_SYSMODULE_INTERNAL_APPINSTUTIL 0x80000014
#define SCE_SYSMODULE_INTERNAL_NET 0x8000001C
#define SCE_SYSMODULE_INTERNAL_IPMI 0x8000001D
#define SCE_SYSMODULE_INTERNAL_VIDEO_OUT 0x80000022
#define SCE_SYSMODULE_INTERNAL_BGFT 0x8000002A
#define SCE_SYSMODULE_INTERNAL_PRECOMPILED_SHADERS 0x80000064

#define SCE_LNC_ERROR_APP_NOT_FOUND 0x80940031 // Usually happens if you to launch an app not in app.db
#define SCE_LNC_UTIL_ERROR_ALREADY_INITIALIZED 0x80940018
#define SCE_LNC_UTIL_ERROR_ALREADY_RUNNING 0x8094000c
#define SCE_LNC_UTIL_ERROR_ALREADY_RUNNING_KILL_NEEDED 0x80940010
#define SCE_LNC_UTIL_ERROR_ALREADY_RUNNING_SUSPEND_NEEDED 0x80940011
#define SCE_LNC_UTIL_ERROR_APP_ALREADY_RESUMED 0x8094001e
#define SCE_LNC_UTIL_ERROR_APP_ALREADY_SUSPENDED 0x8094001d
#define SCE_LNC_UTIL_ERROR_APP_NOT_IN_BACKGROUND 0x80940015
#define SCE_LNC_UTIL_ERROR_APPHOME_EBOOTBIN_NOT_FOUND 0x80940008
#define SCE_LNC_UTIL_ERROR_APPHOME_PARAMSFO_NOT_FOUND 0x80940009
#define SCE_LNC_UTIL_ERROR_CANNOT_RESUME_INITIAL_USER_NEEDED 0x80940012
#define SCE_LNC_UTIL_ERROR_DEVKIT_EXPIRED 0x8094000b
#define SCE_LNC_UTIL_ERROR_IN_LOGOUT_PROCESSING 0x8094001a
#define SCE_LNC_UTIL_ERROR_IN_SPECIAL_RESUME 0x8094001b
#define SCE_LNC_UTIL_ERROR_INVALID_PARAM 0x80940005
#define SCE_LNC_UTIL_ERROR_INVALID_STATE 0x80940019
#define SCE_LNC_UTIL_ERROR_INVALID_TITLE_ID 0x8094001c
#define SCE_LNC_UTIL_ERROR_LAUNCH_DISABLED_BY_MEMORY_MODE 0x8094000d
#define SCE_LNC_UTIL_ERROR_NO_APP_INFO 0x80940004
#define SCE_LNC_UTIL_ERROR_NO_LOGIN_USER 0x8094000a
#define SCE_LNC_UTIL_ERROR_NO_SESSION_MEMORY 0x80940002
#define SCE_LNC_UTIL_ERROR_NO_SFOKEY_IN_APP_INFO 0x80940014
#define SCE_LNC_UTIL_ERROR_NO_SHELL_UI 0x8094000e
#define SCE_LNC_UTIL_ERROR_NOT_ALLOWED 0x8094000f
#define SCE_LNC_UTIL_ERROR_NOT_INITIALIZED 0x80940001
#define SCE_LNC_UTIL_ERROR_OPTICAL_DISC_DRIVE 0x80940013
#define SCE_LNC_UTIL_ERROR_SETUP_FS_SANDBOX 0x80940006
#define SCE_LNC_UTIL_ERROR_SUSPEND_BLOCK_TIMEOUT 0x80940017
#define SCE_LNC_UTIL_ERROR_VIDEOOUT_NOT_SUPPORTED 0x80940016
#define SCE_LNC_UTIL_ERROR_WAITING_READY_FOR_SUSPEND_TIMEOUT 0x80940021
#define SCE_APP_INSTALLER_ERROR_DISC_NOT_INSERTED 0x80A30005
#define SCE_SYSCORE_ERROR_LNC_INVALID_STATE 0x80aa000a
#define SCE_LNC_UTIL_ERROR_NOT_INITIALIZED 0x80940001
#define ORBIS_KERNEL_EAGAIN 0x80020023
#define SFO_PATCH_FLAG_REMOVE_COPY_PROTECTION (1 << 0)
#define SFO_ACCOUNT_ID_SIZE 8
#define SFO_PSID_SIZE 16
#define SFO_DIRECTORY_SIZE 32

#define asset_path(x) "/mnt/sandbox/pfsmnt/" ITEMZFLOW_TID "-app0/assets/" x
#define APP_PATH(x) "/user/app/ITEM00001/" x

#else // on linux
#include <GLES2/gl2.h>

#define APP_PATH(x) "./app_path/" x
#define asset_path(x) "./assets/" x

#endif // defined (__ORBIS__)

#define APP_HOME_DATA_FOLDER "/user/app/NPXS29998"
#define APP_HOME_DATA_TID "NPXS29998"
#define GL_CHECK(stmt) if(glGetError() != GL_NO_ERROR) msgok(FATAL, "GL_STATEMENT %s: %x", getLangSTR(FAILED_W_CODE),glGetError());
#define PS4_OK 0
#define INIT_FAILED -1
#define DEBUG_SETTINGS_TID "NPXS20993"

#define SCE_KERNEL_MAX_MODULES 256
#define SCE_KERNEL_MAX_NAME_LENGTH 256
#define SCE_KERNEL_MAX_SEGMENTS 4
#define SCE_KERNEL_NUM_FINGERPRINT 20

#ifndef ARRAY_SIZE
#	define ARRAY_SIZE(ar) (sizeof(ar) / sizeof((ar)[0]))
#endif


#define IS_INTERNAL 0
#define DIFFERENT_HASH true
#define SAME_HASH false

#define DKS_TIMEOUT 0x804101E2

#define UPDATE_NEEDED true
#define UPDATE_NOT_NEEDED false
#define TRUE 1
#define FALSE 0


#define VERSION_MAJOR 1
#define VERSION_MINOR 02

#define BUILD_YEAR_CH0 (__DATE__[ 7])
#define BUILD_YEAR_CH1 (__DATE__[ 8])
#define BUILD_YEAR_CH2 (__DATE__[ 9])
#define BUILD_YEAR_CH3 (__DATE__[10])


#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')


#define BUILD_MONTH_CH0 \
    ((BUILD_MONTH_IS_OCT || BUILD_MONTH_IS_NOV || BUILD_MONTH_IS_DEC) ? '1' : '0')

#define BUILD_MONTH_CH1 \
    ( \
        (BUILD_MONTH_IS_JAN) ? '1' : \
        (BUILD_MONTH_IS_FEB) ? '2' : \
        (BUILD_MONTH_IS_MAR) ? '3' : \
        (BUILD_MONTH_IS_APR) ? '4' : \
        (BUILD_MONTH_IS_MAY) ? '5' : \
        (BUILD_MONTH_IS_JUN) ? '6' : \
        (BUILD_MONTH_IS_JUL) ? '7' : \
        (BUILD_MONTH_IS_AUG) ? '8' : \
        (BUILD_MONTH_IS_SEP) ? '9' : \
        (BUILD_MONTH_IS_OCT) ? '0' : \
        (BUILD_MONTH_IS_NOV) ? '1' : \
        (BUILD_MONTH_IS_DEC) ? '2' : \
        /* error default */    '?' \
    )

#define BUILD_DAY_CH0 ((__DATE__[4] >= '0') ? (__DATE__[4]) : '0')
#define BUILD_DAY_CH1 (__DATE__[ 5])



// Example of __TIME__ string: "21:06:19"
//                              01234567

#define BUILD_HOUR_CH0 (__TIME__[0])
#define BUILD_HOUR_CH1 (__TIME__[1])

#define BUILD_MIN_CH0 (__TIME__[3])
#define BUILD_MIN_CH1 (__TIME__[4])

#define BUILD_SEC_CH0 (__TIME__[6])
#define BUILD_SEC_CH1 (__TIME__[7])


#if VERSION_MAJOR > 100

#define VERSION_MAJOR_INIT \
    ((VERSION_MAJOR / 100) + '0'), \
    (((VERSION_MAJOR % 100) / 10) + '0'), \
    ((VERSION_MAJOR % 10) + '0')

#elif VERSION_MAJOR > 10

#define VERSION_MAJOR_INIT \
    ((VERSION_MAJOR / 10) + '0'), \
    ((VERSION_MAJOR % 10) + '0')

#else

#define VERSION_MAJOR_INIT \
    (VERSION_MAJOR + '0')

#endif

#if VERSION_MINOR > 100

#define VERSION_MINOR_INIT \
    ((VERSION_MINOR / 100) + '0'), \
    (((VERSION_MINOR % 100) / 10) + '0'), \
    ((VERSION_MINOR % 10) + '0')

#elif VERSION_MINOR > 10

#define VERSION_MINOR_INIT \
    ((VERSION_MINOR / 10) + '0'), \
    ((VERSION_MINOR % 10) + '0')

#else

#define VERSION_MINOR_INIT \
    (VERSION_MINOR + '0')
#endif
// reuse this type to index texts around!
/// for icons.c, sprite.c
#define NUM_OF_TEXTURES  (8)
#define NUM_OF_SPRITES   (6)
#define ITEMZ_LOG "/user/app/ITEM00001/logs/itemzflow_app.log"
/// from GLES2_rects.c

typedef struct sfo_context_s sfo_context_t;

typedef struct sfo_key_pair_s {
    const char* name;
    int flag;
} sfo_key_pair_t;

typedef struct {
    uint32_t flags;
    uint32_t user_id;
    uint64_t account_id;
    uint8_t* psid;
    char* directory;
    char* title_id;
    char* title_name;
} sfo_patch_t;

typedef struct {
    uint32_t unk1;
    uint32_t user_id;
} sfo_params_ids_t;
//#define SFO_MAGIC   0x46535000u
#define SFO_VERSION 0x0101u /* 1.1 */

typedef struct sfo_header_s {
    uint32_t magic;
    uint32_t version;
    uint32_t key_table_offset;
    uint32_t data_table_offset;
    uint32_t num_entries;
} sfo_header_t;

typedef struct list_node_s
{
    void* value;
    struct list_node_s* next;
} list_node_t;

typedef struct list_s
{
    list_node_t* head;
    size_t count;
} list_t;

typedef struct sfo_index_table_s {
    uint16_t key_offset;
    uint16_t param_format;
    uint32_t param_length;
    uint32_t param_max_length;
    uint32_t data_offset;
} sfo_index_table_t;

typedef struct sfo_param_params_s {
    uint32_t unk1;
    uint32_t user_id;
    uint8_t unk2[32];
    uint32_t unk3;
    char title_id_1[0x10];
    char title_id_2[0x10];
    uint32_t unk4;
    uint8_t chunk[0x3B0];
} sfo_param_params_t;

typedef struct sfo_context_param_s {
    char* key;
    uint16_t format;
    uint32_t length;
    uint32_t max_length;
    size_t actual_length;
    uint8_t* value;
} sfo_context_param_t;

struct sfo_context_s {
    list_t* params;
};

typedef enum {
    SFO_SAVE_RESIGN = 0,
    SFO_CHANGE_GAME_NAME
} SFO_PATH_OPT;

enum Settings_options
{
    CDN_SETTING,
    TMP__SETTING,
    HOME_MENU_SETTING,
    INI_SETTING,
    FNT__SETTING,
    STORE_USB_SETTING,
    CLEAR_CACHE_SETTING,
    SHOW_INSTALL_PROG,
    USE_PIXELSHADER_SETTING,
    SAVE_SETTINGS,
    NUM_OF_SETTINGS
};

int pingtest(int libnetMemId, int libhttpCtxId, const char* src);
int32_t netInit(void);

/// from fileIO.c
void queue_panel_init(void);
int  thread_find_by_item(int req_idx);
int  thread_find_by_status(int req_idx, int req_status);
int  thread_count_by_status(int req_status);
int  thread_dispatch_index(void);

/// from GLES2_badges
int  scan_for_badges(layout_t* l, item_t* apps);
void GLES2_Init_badge(void);
void GLES2_Render_badge(int idx, vec4* rect);

/// GLES2_filemamager
void fw_action_to_fm(int button);
void GLES2_render_filemanager(int unused);
void GLES2_render_queue(layout_t* l, int used);

// UI panels new way, v3
vec4 get_rect_from_index(const int idx, const layout_t* l, vec4* io);
bool filter_entry_on_IDs(const char* entry);
bool patch_sfo(const char* in_file_path, sfo_patch_t* patches);