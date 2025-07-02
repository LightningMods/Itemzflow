#include <utils.hpp>
#include <signal.h>
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include "jsmn.h"
#include <dirent.h>
#include "md5.h"
#include <sys/mount.h>
#include <sqlite3.h>
#include <ps4sdk.h>

sqlite3 *db;
#define SSL_HEAP_SIZE	(304 * 1024)
#define HTTP_HEAP_SIZE	(1024 * 1024)
#define NET_HEAP_SIZE   (1 * 1024 * 1024)
#define TEST_USER_AGENT	"Daemon/PS4"

#define VERSION_MAJOR 1
#define VERSION_MINOR 03

const unsigned char completeVersion[] = {
  VERSION_MAJOR_INIT,
  '.',
  VERSION_MINOR_INIT,
  '-',
  'V',
  '-',
  BUILD_YEAR_CH0,
  BUILD_YEAR_CH1,
  BUILD_YEAR_CH2,
  BUILD_YEAR_CH3,
  '-',
  BUILD_MONTH_CH0,
  BUILD_MONTH_CH1,
  '-',
  BUILD_DAY_CH0,
  BUILD_DAY_CH1,
  'T',
  BUILD_HOUR_CH0,
  BUILD_HOUR_CH1,
  ':',
  BUILD_MIN_CH0,
  BUILD_MIN_CH1,
  ':',
  BUILD_SEC_CH0,
  BUILD_SEC_CH1,
  '\0'
};

#define DIFFERENT_HASH true
#define SAME_HASH false
#define UPDATE_NEEDED true
#define UPDATE_NOT_NEEDED false


static int (*jailbreak_me)(void) = NULL;
static int (*rejail_multi)(void) = NULL;

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


int libjbc_module = 0;

int PKG_ERROR(const char* name, int ret)
{
    log_error( "%s error: %i", name, ret);
    return ret;
}

void* prx_func_loader(const char* prx_path, const char* symbol) {

    void* addrp = NULL;
    log_info("Loading %s", prx_path);
     
    int libcmi = sceKernelLoadStartModule(prx_path, 0, NULL, 0, 0, 0);
    if(libcmi < 0){
        log_error("Error loading prx: 0x%X", libcmi);
        return addrp;
    }
    else
        log_debug("%s loaded successfully", prx_path);

    if(sceKernelDlsym(libcmi, symbol, &addrp) < 0){
        log_error("Symbol %s NOT Found", symbol);
    }
    else
        log_debug("Function %s | addr %p loaded successfully", symbol, addrp);

    return addrp;
}

/* we use bgft heap menagement as init/fini as flatz already shown at 
 * https://github.com/flatz/ps4_remote_pkg_installer/blob/master/installer.c
 */

#define BGFT_HEAP_SIZE (1 * 1024 * 1024)

static bool   s_app_inst_util_initialized = false;
static bool   s_bgft_initialized = false;
static struct bgft_init_params  s_bgft_init_params;

bool app_inst_util_init(void) {
    int ret;

    if (s_app_inst_util_initialized) {
        goto done;
    }

    ret = sceAppInstUtilInitialize();
    if (ret) {
        log_debug( "sceAppInstUtilInitialize failed: 0x%08X", ret);
        goto err;
    }

    s_app_inst_util_initialized = true;

done:
    return true;

err:
    s_app_inst_util_initialized = false;

    return false;
}

void app_inst_util_fini(void) {
    int ret;

    if (!s_app_inst_util_initialized) {
        return;
    }

    ret = sceAppInstUtilTerminate();
    if (ret) {
        log_debug( "sceAppInstUtilTerminate failed: 0x%08X", ret);
    }

    s_app_inst_util_initialized = false;
}

bool bgft_init(void) {
    int ret;

    if (s_bgft_initialized) {
        goto done;
    }

    memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));
    {
        s_bgft_init_params.heapSize = BGFT_HEAP_SIZE;
        s_bgft_init_params.heap = (uint8_t*)malloc(s_bgft_init_params.heapSize);
        if (!s_bgft_init_params.heap) {
            log_debug( "No memory for BGFT heap.");
            goto err;
        }
        memset(s_bgft_init_params.heap, 0, s_bgft_init_params.heapSize);
    }

    ret = sceBgftServiceInit(&s_bgft_init_params);
    if (ret) {
        log_debug( "sceBgftInitialize failed: 0x%08X", ret);
        goto err_bgft_heap_free;
    }

    s_bgft_initialized = true;

done:
    return true;

err_bgft_heap_free:
    if (s_bgft_init_params.heap) {
        free(s_bgft_init_params.heap);
        s_bgft_init_params.heap = NULL;
    }

    memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));

err:
    s_bgft_initialized = false;

    return false;
}

void bgft_fini(void) {
    int ret;

    if (!s_bgft_initialized) {
        return;
    }

    ret = sceBgftServiceTerm();
    if (ret) {
        log_debug( "sceBgftServiceTerm failed: 0x%08X", ret);
    }

    if (s_bgft_init_params.heap) {
        free(s_bgft_init_params.heap);
        s_bgft_init_params.heap = NULL;
    }

    memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));

    s_bgft_initialized = false;
}
/* sample ends */


/* install package wrapper:
   init, (install), then clean AppInstUtil and BGFT
   for next install */

uint32_t pkginstall(const char *path, bool is_if_update)
{
    int  ret = -1;
    int  task_id = -1;
    int is_app = false;
    char title_id[30];
    SceBgftTaskProgress progress_info;

    if( if_exists(path) )
    {
        log_info("Initializing AppInstUtil...");
        if (!app_inst_util_init()) {
            log_debug( "AppInstUtil initialization failed.");
            return -1;
        }
        log_info("Initializing BGFT...");
        if (!bgft_init()) {
            log_debug( "BGFT initialization failed.");
             return -1;
        }

        struct bgft_download_param_ex download_params;
        memset(&download_params, 0, sizeof(download_params));
        download_params.param.entitlement_type = 5;
        download_params.param.id = "";
        download_params.param.content_url = path;
        if(is_if_update)
           download_params.param.content_name = "ItemzFlow Update";
        else
           download_params.param.content_name = "PKG Installed remotely";

        download_params.param.icon_path = "/update/fakepic.png";
        download_params.param.playgo_scenario_id = "0";
        download_params.param.option = BGFT_TASK_OPTION_DELETE_AFTER_UPLOAD;
        download_params.slot = 0;

        ret = sceAppInstUtilGetTitleIdFromPkg(path, &title_id[0], &is_app);
        if (ret) 
            return PKG_ERROR("sceAppInstUtilGetTitleIdFromPkg", ret);

retry:
        ret = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&download_params, &task_id);
        if(ret == 0x80990088 || ret == 0x80990015)
        {
            ret = sceAppInstUtilAppUnInstall(&title_id[0]);
            if(ret != 0)
                return PKG_ERROR("sceAppInstUtilAppUnInstall", ret);

            log_info("Uninstalled %s, retrying install...", &title_id[0]);
            goto retry;
        }
        else if(ret)
            return PKG_ERROR("sceBgftDownloadRegisterTaskByStorageEx", ret);

        log_info("Task ID(s): 0x%08X", task_id);

        ret = sceBgftServiceDownloadStartTask(task_id);
        if(ret) 
            return PKG_ERROR("sceBgftDownloadStartTask", ret);

        int prog = 0;
        while (prog < 99) {
            memset(&progress_info, 0, sizeof(progress_info));

            int ret = sceBgftServiceDownloadGetProgress(task_id, &progress_info);
            if (ret) 
                return PKG_ERROR("sceBgftDownloadGetProgress", ret);
            
            prog = (uint32_t)(((float)progress_info.transferred / progress_info.length) * 100.f); 
        }

    }
    else
        return PKG_ERROR("no file at", ret);

    return 0;
}


bool rejail()
{
    int ret = sys_dynlib_dlsym(libjbc_module, "rejail_multi", &rejail_multi);
    if (!ret)
    {
        log_info("[Daemon] rejail_multi resolved from PRX");

        if ((ret = rejail_multi() != 0)) return false;
        else
            return true;
    }
    else
    {
        // fail con
        log_debug("[Daemon] rejail_multi failed to resolve");
        return false;
    }

    return false;
}

bool jailbreak(const char* prx_path)
{
    sys_dynlib_load_prx((char*)prx_path, &libjbc_module);
    int ret = sys_dynlib_dlsym(libjbc_module, "jailbreak_me", &jailbreak_me);
    if (!ret)
    {
        log_info("[Daemon] jailbreak_me resolved from PRX");

        if ((ret = jailbreak_me() != 0))
        {
            log_debug("[Daemon] jailbreak_me returned %i", ret);
            return false;
        }
        else
            return true;
    }
    else
      log_debug("[Daemon] jailbreak_me failed to resolve");
 
    return false;
}


bool if_exists(const char* path)
{
    int dfd = open(path, O_RDONLY, 0); // try to open dir
    if (dfd < 0) {
        log_info("path %s, errno %s", path, strerror(errno));
        return false;
    }
    else
        close(dfd);


    return true;
}
bool reboot_daemon = false;

int sceKernelBacktraceSelf(callframe_t frames[], size_t numBytesBuffer, uint32_t *pNumReturn, int mode);

void OrbisKernelBacktrace(const char* reason) {
    char addr2line[MAX_STACK_FRAMES * 20];
    callframe_t frames[MAX_STACK_FRAMES];
    OrbisKernelVirtualQueryInfo info;
    char buf[MAX_MESSAGE_SIZE + 3];
    unsigned int nb_frames = 0;
    char temp[80];

    memset(addr2line, 0, sizeof addr2line);
    memset(frames, 0, sizeof frames);
    memset(buf, 0, sizeof buf);

    snprintf(buf, sizeof buf, "<118>[Crashlog]: %s\n", reason);

    strncat(buf, "<118>[Crashlog]: Backtrace:\n", MAX_MESSAGE_SIZE);
    sceKernelBacktraceSelf(frames, sizeof frames, &nb_frames, 0);
    for (unsigned int i = 0; i < nb_frames; i++) {
        memset(&info, 0, sizeof info);
        sceKernelVirtualQuery(frames[i].pc, 0, &info, sizeof info);

        snprintf(temp, sizeof temp,
            "<118>[Crashlog]:   #%02d %32s: 0x%lx\n",
            i + 1, info.name, frames[i].pc - info.unk01 - 1);
        strncat(buf, temp, MAX_MESSAGE_SIZE);

        snprintf(temp, sizeof temp,
            "0x%lx ", frames[i].pc - info.unk01 - 1);
        strncat(addr2line, temp, sizeof addr2line - 1);
    }

    strncat(buf, "<118>[Crashlog]: addr2line: ", MAX_MESSAGE_SIZE);
    strncat(buf, addr2line, MAX_MESSAGE_SIZE);
    strncat(buf, "\n", MAX_MESSAGE_SIZE);

    buf[MAX_MESSAGE_SIZE + 1] = '\n';
    buf[MAX_MESSAGE_SIZE + 2] = '\0';

    log_error("%s", buf);
}


bool mount_procFS(){
    char profpath[150];

    if(getuid() != 0){
        log_debug("Not root, skipping procfs mount");
        return false;
    }

    snprintf(profpath, sizeof(profpath)-1, "/mnt/proc/%i/", getpid());

    if (getuid() == 0 && !if_exists(profpath))
    {
        int result = mkdir("/mnt/proc", 0777);
        if (result < 0 && errno != EEXIST)
        {
            log_debug("Failed to create /mnt/proc");
            return false;
        }

        result = mount("procfs", "/mnt/proc", 0, NULL);
        if (result < 0 && errno != EBUSY)
        {
            log_debug("Failed to mount procfs: %s", strerror(errno));
            return false;
        }
    }
    log_info("# Procfs mounted to %s", profpath);
    return true;
}


void SIG_Handler(int sig_numb)
{
   // void* array[100];
    if(sig_numb != SIGQUIT){
       sceKernelIccSetBuzzer(2);

    log_debug("############# DAEMON HAS CRASHED with SIG %i ##########", sig_numb);
    log_debug("# Thread ID: %i", pthread_getthreadid_np());
    log_debug("# PID: %i", getpid());
    log_debug("# UID: %i", getuid());

    char buff[255];
    if (mount_procFS())
    {
       FILE* fp = fopen("/mnt/proc/curproc/status", "r");
       if (fp == NULL) {
          log_error("Failed to open file");
          goto bt;
       }
       if (fscanf(fp, "%254s", buff) != 1) {
           log_error("Failed to read string from file");
           fclose(fp);
           goto bt;
        }

        log_debug("# Reading curproc...");
        log_debug("# Proc Name: %s", buff);
        fclose(fp);
    }
bt:
    log_debug("###################  Backtrace  ########################");
    //backtrace(array, 100);
    OrbisKernelBacktrace("SIG_Handler");
    notify("ItemzDaemon has crashed, exiting...");
    }

    rejail();
    if(reboot_daemon)
        sceSystemServiceLoadExec("/app0/eboot.bin", 0);
    else
       sceSystemServiceLoadExec("exit", 0);

}



bool MD5_hash_compare(const char* file1, const char* hash)
{
    unsigned char c[MD5_HASH_LENGTH];
    FILE* f1 = fopen(file1, "rb");
    if (f1 == NULL)
        return true;
    MD5_CTX mdContext;

    int bytes = 0;
    unsigned char data[1024];

    MD5_Init(&mdContext);
    while ((bytes = fread(data, 1, 1024, f1)) != 0)
        MD5_Update(&mdContext, data, bytes);
    MD5_Final(c, &mdContext);

    char md5_string[33] = { 0 };

    for (int i = 0; i < 16; ++i) {
        snprintf(&md5_string[i * 2], 32, "%02x", (unsigned int)c[i]);
    }
    log_info("FILE HASH: %s", md5_string);
    md5_string[32] = 0;

    if (strcmp(md5_string, hash) != 0) {
        return DIFFERENT_HASH;
    }

    log_info("Input HASH: %s", hash);
    fclose(f1);

    return SAME_HASH;
}

int netInit(void)
{
    // CURL SOMEDAY
    return 0;
}


#include <pthread.h>


void Network_Init()
{
    int ret = netInit();
    if (ret < 0) { log_info("netInit() error: 0x%08X", ret); }
}

int jsoneq(const char* json, jsmntok_t* tok, const char* s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

static char *checkedsize = NULL;

long CalcAppsize(const char *path)
{
    FILE *fp = NULL;
    long off;

    fp = fopen(path, "r");
    if (fp == NULL) return 0;
            
    if (fseek(fp, 0, SEEK_END) == -1) goto cleanup;
            
    off = ftell(fp);
    if (off == (long)-1) goto cleanup;

    if (fclose(fp) != 0) goto cleanup;
    
    if(off) { return off; }
    else    { checkedsize = NULL; return 0; }

cleanup:
    if (fp != NULL)
        fclose(fp);

    return 0;

}

void save_fuse_ip(const char* ip) {
    
    FILE* fp = fopen("/data/itemzflow_daemon/fuse_ip.txt", "w");
    if(fp == NULL) 
       return;
    fprintf(fp, "%s", ip);
    fclose(fp);
}

enum{
    MAIN_ON_STANDBY = 500,
    WORKING = 1000,
};

bool isRestMode()
{
    return sceSystemStateMgrGetCurrentState() == MAIN_ON_STANDBY;
}

bool IsOn()
{
    //return (unsigned int)sceSystemStateMgrGetCurrentState() == WORKING;
    return true;
}

int sceSysUtilSendSystemNotificationWithText(int messageType, char* message);

void notify(const char* message)
{
    log_info(message);
    sceKernelLoadStartModule("/system/common/lib/libSceSysUtil.sprx", 0, NULL, 0, 0, 0);
    sceSysUtilSendSystemNotificationWithText(222, message);
}  

bool create_stat_db(const char* path){
    char *err_msg = 0;
    
    if(!if_exists("/user/app/ITEM00001/"))
       mkdir("/user/app/ITEM00001/", 0777);

    mkdir("/user/app/ITEM00001/game_stats/", 0777);
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        log_error("[STATS] Cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
    
    const char *smt = "CREATE TABLE Game_Stats(tid TEXT, mins_played INT, first_started INT, PRIMARY KEY(tid));";
    rc = sqlite3_exec(db, smt, 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        log_error("[STATS] SQL error: %s", err_msg);
        sqlite3_free(err_msg);        
        sqlite3_close(db);
        return false;
    } 
    
    sqlite3_close(db);
    return true;
}


void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

#define ORBIS_LIBC_MALLOC_MANAGED_SIZE_VERSION (0x0001U)

#ifndef SCE_LIBC_INIT_MALLOC_MANAGED_SIZE
#define SCE_LIBC_INIT_MALLOC_MANAGED_SIZE(mmsize) do { \
	mmsize.size = sizeof(mmsize); \
	mmsize.version = ORBIS_LIBC_MALLOC_MANAGED_SIZE_VERSION; \
	mmsize.reserved1 = 0; \
	mmsize.maxSystemSize = 0; \
	mmsize.currentSystemSize = 0; \
	mmsize.maxInuseSize = 0; \
	mmsize.currentInuseSize = 0; \
} while (0)
#endif

void* statemgr = NULL;

int sceSystemOpenStartMgr(){
  if ( (unsigned int)sceKernelOpenEventFlag(&statemgr, "SceSystemStateMgrInfo") )
    return -1;

  return 0;
}
#define SCE_KERNEL_EVF_WAITMODE_OR 0x02

int sceSystemStateMgrGetCurrentState()
{
  uint64_t ret = 0;

  if(!statemgr){
     if(sceSystemOpenStartMgr() == -1)
        return -1;
  }
  sceKernelPollEventFlag(statemgr, 0xFFFF, SCE_KERNEL_EVF_WAITMODE_OR, &ret);
  if((int)ret == WORKING && sceKernelPollEventFlag(statemgr, 0x200000, SCE_KERNEL_EVF_WAITMODE_OR, 0) == 0)
      ret = MAIN_ON_STANDBY;
  // my custom check
  // int MAppid = sceSystemServiceGetAppIdOfMiniApp();
  // if (0 < MAppid)
  //    ret = MAIN_ON_STANDBY;
  if((int)ret == MAIN_ON_STANDBY)
     log_debug("SceSystemStateMgrGetCurrentState MAIN_ON_STANDBY");

  return (int)ret;
}

void print_memory()
{
   LibcMallocManagedSize ManagedSize;
   SCE_LIBC_INIT_MALLOC_MANAGED_SIZE(ManagedSize);
   malloc_stats_fast(&ManagedSize);
   log_info("[MEM_MANAGER] ================================================]");
   log_info("[MEM] CurrentSystemSize: %i, CurrentInUseSize: %i", ManagedSize.currentSystemSize, ManagedSize.currentInuseSize);
   log_info("[MEM] MaxSystemSize: %i, MaxInUseSize: %i", ManagedSize.maxSystemSize, ManagedSize.maxInuseSize);
   log_info("[MEM_MANAGER] ================================================]");

}

bool open_stat_db(const char* path){
    int ret;
    struct stat sb;

    // check if our sqlite db is already present in our path
    ret = sceKernelStat(path, &sb);
    if (ret != 0)
    {
        log_error("[ERROR][SQLLDR][%s][%d] sceKernelStat(%s) failed: 0x%08X", __FUNCTION__, __LINE__, path, ret);
        return false;
    }
    ret = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL);
    if (ret != SQLITE_OK)
    {
        log_error("[ERROR][SQLLDR][%s][%d] ERROR 0x%08X: %s", __FUNCTION__, __LINE__, ret, sqlite3_errmsg(db));
        return false;
    }

    return true;
}

bool update_game_time_played(const char* tid){

    char smt[200];
    char *err_msg = NULL;

    if(!tid && strlen(tid) != 9){
       log_error("[STATS] Invalid tid");
       return false;
    }
 
    snprintf(smt, sizeof(smt)-1, "INSERT OR IGNORE INTO Game_Stats(tid, mins_played, first_started) VALUES('%s', 0, '%ld')", tid, (long)time(NULL));

    int rc = sqlite3_exec(db, &smt[0], 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        log_error("[STATS] SQL error: %s", err_msg);
        sqlite3_free(err_msg);        
        return false;
    } 

    snprintf(smt, sizeof(smt)-1, "UPDATE Game_Stats SET mins_played = mins_played+1 WHERE tid = '%s'", tid);

    rc = sqlite3_exec(db, &smt[0], 0, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        log_error("[STATS] SQL error: %s", err_msg);
        sqlite3_free(err_msg);        
        return false;
    } 
    
    return true;
}

int int_cb(void *in, int idc, char **argv, char **idc_2)
{
    int *out = (int *)in;
    *out = atoi(argv[0]);
    return 0;
}
/*
static int token_cb(void *in, int count, char **data, char **columns)
{
    char **result_str = (char **)in;
    *result_str = sqlite3_mprintf("%w", data[0]);

    return 0;
}
*/
int get_game_time_played(const char* tid){
    char smt[200];
    char *err_msg = NULL;
    int mins_played = -1;
    
    snprintf(smt, sizeof(smt)-1, "SELECT mins_played FROM Game_Stats WHERE tid = '%s'", tid);
    
    int rc = sqlite3_exec(db, &smt[0], int_cb, &mins_played, &err_msg);
    if (rc != SQLITE_OK ) {
        log_error("[STATS] SQL error: %s", err_msg);
        sqlite3_free(err_msg);        
        return -1;
    } 
    
    return mins_played;
}

time_t get_game_start_date(const char* tid){
    char smt[200];
    char* err_msg = NULL;
    time_t epoch = 0;
    
    snprintf(smt, sizeof(smt)-1, "SELECT first_started FROM Game_Stats WHERE tid = '%s'", tid);
    
    int rc = sqlite3_exec(db, &smt[0], int_cb, &epoch, &err_msg);
    if (rc != SQLITE_OK ) {
        log_error("[STATS] SQL error: %s", err_msg);
        sqlite3_free(err_msg);        
        return 0;
    } 
    
    return epoch;
}
bool copyRegFile(const char* source, const char* dest)
{
    
    int src = sceKernelOpen(source, 0x0000, 0);
    if (src > 0)
    {
        
        int out = sceKernelOpen(dest, 0x0001 | 0x0200 | 0x0400, 0777);
        if (out > 0)
        {   
            size_t bytes = 0;
            char buf[4096];
            while (0 < (bytes = sceKernelRead(src, &buf[0], 4096))){
                sceKernelWrite(out, &buf[0], bytes);
                memset(&buf[0], 0, 4096);
            }
        }
        else
        {
            sceKernelClose(src);
            return false;
        }

        sceKernelClose(src);
        sceKernelClose(out);
        return true;
    }
    else
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
        snprintf(fname, sizeof(fname)-1, "%s/%s", path, de->d_name);
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


bool copy_dir(const char* sourcedir,const char* destdir) {

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
                    if (!copyRegFile(src_path, dst_path))
                        return false;
                }
            }
        }
    }
    closedir(dir);

    return true;
}


bool full_init()
{
    pthread_t fuse_start_thread;
    
    // pad
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PAD)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SYSUTIL)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_NETCTL)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_NET)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_HTTP)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SSL)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SYS_CORE)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_BGFT)) return false;
    if (sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_APPINSTUTIL)) return false;

    mkdir("/data/itemzflow_daemon", 0777);
    mkdir("/mnt/usb0/itemzflow", 0777);
    unlink(DAEMON_LOG_PS4);
    // get fuse ip

    /*-- INIT LOGGING FUNCS --*/
    log_set_quiet(false);
    log_set_level(LOG_DEBUG);
    FILE* fp = fopen(DAEMON_LOG_PS4, "w");
    log_add_fp(fp, LOG_DEBUG);

    if (touch_file(DAEMON_LOG_USB))
    {
        fp = fopen(DAEMON_LOG_USB, "w");
        log_add_fp(fp, LOG_DEBUG);
    }
    /* -- END OF LOGINIT --*/
    log_info("------------------------ ItemzFlow[Daemon] Compiled Time: %s @ %s  -------------------------", __DATE__, __TIME__);
    log_info(" ---------------------------  Daemon Version: %s ------------------------------------", completeVersion);

    //Dump code and sig hanlder
    struct sigaction new_SIG_action;

    new_SIG_action.sa_handler = SIG_Handler;
    sigemptyset(&new_SIG_action.sa_mask);
    new_SIG_action.sa_flags = 0;
    //for now just SEGSEV
    for (int i = 0; i < 12; i++)
        sigaction(i, &new_SIG_action, NULL);
    
    sceSystemServiceHideSplashScreen();

    if(sceSystemOpenStartMgr() == -1){
       log_error("Failed to open start state manager");
       notify("Failed to open start state manager");
    }

    pthread_create(&fuse_start_thread, NULL, fuse_startup, NULL);

    return true;
}
