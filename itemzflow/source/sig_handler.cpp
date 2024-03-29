
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>  // malloc, qsort, free
#include <unistd.h>  // close
#include <sys/signal.h>
#include <errno.h>
#include <utils.h>
//#include <sys/mount.h>
#include <dumper.h>
#include <utils.h>
#include "lang.h"
#include <libkernel.h>
#include "feature_classes.hpp"
#include <sys/mman.h>




extern "C" int pthread_getthreadid_np();
#define OBRIS_BACKTRACE 1
#define SYS_mount 21
using namespace multi_select_options;

extern "C" int sys_mount(const char *type, const char	*dir, int flags, void *data){
    return syscall(SYS_mount, type, dir, flags, data);
}

extern ItemzSettings set,
* get;

extern char* gm_p_text[20];
extern "C" int backtrace(void **buffer, int size);
Dump_Multi_Sels dump = SEL_DUMP_ALL;
// IPC IS USING C
extern std::string ipc_msg;

int wait_for_game(char *title_id)
{
    int res = 0;

    DIR *dir;
    struct dirent *dp;


    dir = opendir("/mnt/sandbox/pfsmnt");
    if (!dir)
        return 0;


    while ((dp = readdir(dir)) != NULL)
    {

        log_info("%s", dp->d_name);

        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
        {
            // do nothing (straight logic)
        }
        else
        {
            log_info("%s", dp->d_name);

            if (strstr(dp->d_name, "-app0") != NULL)
            {
                 res = 1;
                break;
            }
        }
    }
    closedir(dir);
    log_info("wait_for_game finish");

    return res;
}

bool dump_to_diff_folder(std::string &title_id, std::string& usb_p, std::string& gtitle, Dump_Multi_Sels dump)
{
    std::string tmp;
    // make a switch case for dump

    print_memory();
    if (dump != SEL_GAME_PATCH || if_exists(fmt::format("/user/patch/{}/patch.pkg", title_id).c_str()))
    {
        switch(dump)
        {
            case SEL_GAME_PATCH: {
               return Dumper(usb_p, title_id, GAME_PATCH, gtitle);
               break;
            }
            case SEL_DLC:{
                return Dumper(usb_p, title_id, ADDITIONAL_CONTENT_DATA, gtitle);
                break;
            }
            default: {
                log_debug("Unknown Dump Type");
                break;
            }
        }
    }
    else
    {
        log_debug("Patch doesnt exist...");
        return true;
    }

    return false;
}

bool Dump_w_opts(std::string tid, std::string d_p, std::string gtitle, Dump_Multi_Sels dump)
{
    std::string tmp =  fmt::format("/mnt/sandbox/pfsmnt/{}-app0-nest/pfs_image.dat", tid);
    if (!if_exists(tmp.c_str())) {
        log_error("Dump_w_opts: %s doesnt exist", tmp.c_str());
        return false;
    }

    if (dump == SEL_DUMP_ALL) {
        if (Dumper(d_p, tid, BASE_GAME, gtitle)) {
            return dump_to_diff_folder(tid, d_p, gtitle, SEL_GAME_PATCH);
        }
        else {
            log_debug("Dumping Base game failed, trying to dump as Remaster...");
            if (Dumper(d_p, tid, REMASTER, gtitle)) {
                return dump_to_diff_folder(tid, d_p, gtitle, SEL_GAME_PATCH);
            }
        }
        
    }
    else if (dump == SEL_BASE_GAME) {
        if (Dumper(d_p, tid, BASE_GAME, gtitle)) {

            tmp = fmt::format("{}/{}.complete", d_p, tid);
            touch_file(tmp.c_str());
            tmp = fmt::format("{}/{}.dumping", d_p, tid);
            unlink(tmp.c_str());

            return true;
        }
    }
    else if (dump == SEL_GAME_PATCH) {
        return dump_to_diff_folder(tid, d_p, gtitle, dump);
    }
    else if (dump == SEL_REMASTER) {
        return Dumper(d_p, tid, REMASTER, gtitle);
    }
    else if (dump == SEL_DLC) {
        return dump_to_diff_folder(tid, d_p, gtitle, dump);
    }

    return false;
}

int (*rejail_multi)(void) = NULL;

void Exit_Success(const char* text)
{
    log_info("%s", text);
    log_info("Disable Home Menu Redirect Called for Exit....");

    if (rejail_multi) {
        log_debug("Rejailing App >>>");
        rejail_multi();
        log_debug("App ReJailed");
    }

    if(reboot_app)
        sceSystemServiceLoadExec("/app0/ItemzCore.self", 0);

    log_debug("Calling SystemService Exit");
    sceSystemServiceLoadExec("exit", 0);
}

void Exit_App_With_Message(int sig_numb)
{
    msgok(MSG_DIALOG::NORMAL, fmt::format("############# Program has gotten a FATAL Signal/Error: {0:d} ##########\n\n {1:}\n\n", sig_numb, getLangSTR(LANG_STR::PKG_TEAM)));

    std::string tmp;

    if (usbpath() != -1) {
       //try our best to make the name not always the same
        tmp = fmt::format("/mnt/usb{0:d}/itemzflow", usbpath());
        mkdir(tmp.c_str(), 0777);
        tmp = fmt::format("{}/crash.log", tmp);

        if (touch_file(tmp.c_str())) {
           copyFile(ITEMZ_LOG, tmp, false);

          if (if_exists(DUMPER_LOG))
              copyFile(DUMPER_LOG, tmp, false);

        }
        else
           log_debug("failed writing log to %s", tmp.c_str());

    }
    else
        log_error("[DUMP] failed to get usbpath");

    Exit_Success("Exit_App_With_Message ->");
}

#if OBRIS_BACKTRACE==1

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
#endif
bool is_dumper_running = false;

void SIG_Handler(int sig_numb)
{
    std::string profpath;
    IPC_Client& ipc = IPC_Client::getInstance();
    rejail_multi = (int (*)(void))prx_func_loader("/app0/Media/jb.prx", "rejail_multi");
    int fw = (ps4_fw_version() >> 16);
    //void* bt_array[255];
    //
    switch (sig_numb)
    {
    case SIGABRT:
        log_debug("ABORT called.");
        goto SIGSEGV_handler;
        break;
    case SIGTERM:
        log_debug("TERM called");
        break;
    case SIGINT:
        log_debug("SIGINT called");
        break;
    case SIGQUIT:
        Exit_Success("SIGQUIT Called");
        break;
    case SIGKILL:
        Exit_Success("SIGKILL Called");
        break;
    case SIGSTOP:
        log_debug("App has been suspeneded");
        goto App_suspended;
        break;
    case SIGCONT:
        log_debug("App has resumed");
        goto App_Resumed;
        break;
    case SIGUSR2:
        log_debug("SIGUSER2 called, Prob from libFMT");
        goto App_Resumed;
        break;
    case SIGSEGV: {
SIGSEGV_handler:
        log_debug("App has crashed");
        if (dump != SEL_RESET)
        {
            msgok(MSG_DIALOG::WARNING, getLangSTR(LANG_STR::FATAL_DUMP_ERROR));
            sceSystemServiceLoadExec("/app0/ItemzCore.self", 0);
        }
        unlink("/user/app/if_update.pkg");
        goto backtrace_and_exit;
    }
    default:

        //Proc info and backtrace
        goto backtrace_and_exit;

        break;
    }


backtrace_and_exit:
    log_debug("backtrace_and_exit called");
    ipc.IPCSendCommand(IPC_Commands::DISABLE_HOME_REDIRECT, ipc_msg);
    
    if(get->setting_bools[INTERNAL_UPDATE])
        mkdir("/user/recovery.flag", 0777);

    profpath =  fmt::format("/mnt/proc/{0:d}/", getpid());
    if (getuid() == 0 && !if_exists(profpath.c_str()))
    {
        int result = mkdir("/mnt/proc", 0777);
        if (result < 0 && errno != 17)
           log_debug("Failed to create /mnt/proc: %s", strerror(errno));
        
        result = sys_mount("procfs", "/mnt/proc", 0, NULL);
        if (result < 0)
            log_debug("Failed to mount procfs: %s", strerror(errno));
    }
    //
    log_debug("############# Program has gotten a FATAL Signal: %i ##########", sig_numb);
    log_debug("# Thread ID: %i", pthread_getthreadid_np());
    log_debug("# PID: %i", getpid());

    if (getuid() == 0)
        log_debug("# mounted ProcFS to %s", profpath.c_str());


    log_debug("# UID: %i", getuid());

    char buff[255];

    if (getuid() == 0)
    {
        FILE* fp;

        fp = fopen("/mnt/proc/curproc/status", "r");
        fscanf(fp, "%s", &buff[0]);
        log_debug("# Reading curproc...");
        log_debug("# Proc Name: %s", &buff[0]);
        fclose(fp);
    }
    log_debug("###################  Backtrace  ########################");

#if OBRIS_BACKTRACE==1
#if __cpplusplus
extern "C" {
#endif
 OrbisKernelBacktrace("Fatal Signal");
#if __cpplusplus
}
#endif
#endif
void* bt_array[255];

if(fw <= 0x505)
   backtrace(bt_array, 255);

Exit_App_With_Message(sig_numb);


    
App_suspended:
log_debug("App Suspended Checking for opened games...");
App_Resumed:
log_debug("App Resumed Checking for opened games...");

//lock thread mutex so it doesnt constantly continue running
//while we are dumping or checking if a game is open
// only unlcok at the end of the func
if(!is_remote_va_launched && app_status.load() != APP_NA_STATUS){
int BigAppid = sceSystemServiceGetAppIdOfBigApp();
if ((BigAppid & ~0xFFFFFF) == 0x60000000) {
    log_info("[SIGNAL] FOUND Open Game: %x : Bool: %d", BigAppid, (BigAppid & ~0xFFFFFF) == 0x60000000);
    if(sceLncUtilGetAppId(title_id.get().c_str()) == sceSystemServiceGetAppIdOfBigApp())
       app_status = RESUMABLE;
    else
       app_status = OTHER_APP_OPEN;
}
else
   log_info("[SIGNAL] No Open Games detected");
}


//  char* title_id.get() = "CUSA02290";
    if ( dump != SEL_RESET)
    {
        is_dumper_running = true;
        std::string tmp;
        std::string tmp2;
        progstart((char*)getLangSTR(LANG_STR::STARTING_DUMPER).c_str());

        fmt::print("title_id.get(): {},  DUMPER_PATH: {}", title_id.get(), get->setting_strings[DUMPER_PATH]);
        tmp = fmt::format("{}/{}", get->setting_strings[DUMPER_PATH], title_id.get());

        if (dump == SEL_BASE_GAME && if_exists(tmp.c_str()))
        {
            ProgSetMessagewText(5, getLangSTR(LANG_STR::DELETING_GAME_FOLDER).c_str());
            rmtree(tmp.c_str());
            tmp = fmt::format("{}/{}.complete", get->setting_strings[DUMPER_PATH], title_id.get());
            unlink(tmp.c_str());
        }

        tmp = fmt::format("{}/{}-patch/", get->setting_strings[DUMPER_PATH], title_id.get());
        if (dump == SEL_GAME_PATCH && if_exists(tmp.c_str()))
        {
            ProgSetMessagewText(5, getLangSTR(LANG_STR::DELETING_GAME_FOLDER).c_str());
            rmtree(tmp.c_str());
        }
        
        //dump_game has all the dialoge for copied proc
        if (Dump_w_opts(title_id.get(), get->setting_strings[DUMPER_PATH], g_idx > all_apps.size() ? "WTF" : all_apps[g_idx].info.name, dump)){
            msgok(MSG_DIALOG::NORMAL, fmt::format("{0:} {1:} {2:}", getLangSTR(LANG_STR::DUMP_OF), title_id.get(), getLangSTR(LANG_STR::COMPLETE_WO_ERRORS)));
        }
        else{

            if (usbpath() != -1) {
              //try our best to make the name not always the same
              tmp = fmt::format("/mnt/usb{}/itemzflow", usbpath());
              mkdir(tmp.c_str(), 0777);
              tmp = fmt::format("{}/dumper.log", tmp);

              if (touch_file(tmp.c_str())) {
                  log_debug("Dump failed writing log to %s", tmp.c_str());
                   copyFile(DUMPER_LOG, tmp, false);
              }
            }
            else
                log_error("[DUMP] failed to get usbpath");
        

            msgok(MSG_DIALOG::WARNING, fmt::format("{0:} {1:} {2:}", getLangSTR(LANG_STR::DUMP_OF), title_id.get(), getLangSTR(LANG_STR::DUMP_FAILED)));
        }

        print_memory();

        log_info("finished");
        is_dumper_running = false;
        dump = SEL_RESET;
        
    }
    if (is_dumper_running){
        msgok(MSG_DIALOG::NORMAL, getLangSTR(LANG_STR::DUMPER_CANCELLED));
        dump = SEL_RESET;
        is_dumper_running = false;
    }
    
}
