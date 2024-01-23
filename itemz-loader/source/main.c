#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <orbisPad.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <MsgDialog.h>

#include "Header.h"
#include <ps4sdk.h>
#include <errno.h>
#include "lang.h"
#include "ini.h"
#include <sys/signal.h>
#include <signal.h>
int ret;
int libnetMemId = 0;
int libsslCtxId = 0;
int libhttpCtxId = 0;
bool restart_nessary = false;

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

    //log_error("%s", buf);
	msgok(buf);	
}

void is_restart_needed_for_daemon(void* arg){

    
	logshit("is_restart_needed_for_daemon (%i)", atoi(arg));
	uint32_t appid = sceLncUtilGetAppId("ITEM00002");
    //Launch Daemon with silent    
    if ((appid & ~0xFFFFFF) != 0x60000000){
        restart_nessary = true;
		logshit("Restart needed for Daemon!");
	}
    else
       logshit("Found Daemon AppId: %x, No restart needed", appid);
}

void SIG_Handler(int sig_numb)
{
	//("App got signal %i", sig_numb);
	OrbisKernelBacktrace("Itemz-loader has Crashed");
	sceSystemServiceLoadExec("exit", 0);

}


int main(int argc, char* argv[])
{
	init_itemzGL_modules();

	int libjb = -1;

	sceSystemServiceHideSplashScreen();
	sceCommonDialogInitialize();
    sceMsgDialogInitialize();

	//msgok("DO YOU SEE THiS??");
	//Dump code and sig hanlder
    struct sigaction new_SIG_action;

   // log_info("Registering Signal handler... ");

   // new_SIG_action.sa_sigaction = SIG_IGN;
    new_SIG_action.sa_handler = SIG_Handler;
    sigemptyset(&new_SIG_action.sa_mask);
    new_SIG_action.sa_flags = 0;

    for (int i = 0; i < 43; i++) {
      //  if(i != SIGQUIT)
          sigaction(i, &new_SIG_action, NULL);
    }


	if (!LoadLangs(PS4GetLang())) {
		 //msgok("DD 1");
		if (!LoadLangs(0x01)) {
			msgok("Failed to load Backup Lang...\nThe App is unable to continue"); 
			goto exit_no_lang;
		}
	}


    libjb = sceKernelLoadStartModule("/app0/Media/jb.prx", 0, NULL, 0, 0, 0);
    int ret = sceKernelDlsym(libjb, "jbc_run_as_root", (void**)&jbc_run_as_root);
    if (ret >= 0){
        if (!jbc_run_as_root){
             msgok("%s: %x", getLangSTR(FATAL_JB),ret); 
			 goto err;
		}
		else{
			//msgok("jbc_run_as_root %p", jbc_run_as_root);
			jbc_run_as_root(loader_rooted, NULL, CWD_ROOT);
		}
    }
	else{
        msgok("%s: %x", getLangSTR(FATAL_JB),ret); goto err;
	}
	
	if(atoi(argv[0]) < 3){
		jbc_run_as_root(is_restart_needed_for_daemon, argv[0], CWD_ROOT);
		if(restart_nessary){
			char buffer[256];
			sprintf(buffer, "%i", atoi(argv[0])+1);
			const char* argv[2] = { buffer, NULL };
			sceSystemServiceLoadExec("/app0/eboot.bin", argv);
		}
	}

	logshit("[Itemz-loader:%s:%i] ----- Launching() ---", __FUNCTION__, __LINE__);
	if (sceSystemServiceLoadExec("/app0/ItemzCore.self", 0) == 0)
		logshit("[Itemz-loader:%s:%i] ----- Launched (shouldnt see) ---", __FUNCTION__, __LINE__);

err:
   msgok("%s\n\n%s\n", getLangSTR(LOADER_ERROR), getLangSTR(MORE_INFO));
exit_no_lang:
   copyFile("/user/app/ITEM00001/logs/loader.log", "/usb0/itemzflow/loader.log");
   mkdir("/user/recovery.flag", 0777);

   return sceSystemServiceLoadExec("exit", 0);     
}

void catchReturnFromMain(int exit_code)
{
}
