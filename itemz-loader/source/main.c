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

int ret;
int libnetMemId = 0;
int libsslCtxId = 0;
int libhttpCtxId = 0;
bool restart_nessary = false;

void is_restart_needed_for_daemon(void* arg){

    
	logshit("is_restart_needed_for_daemon (%i)\n", atoi(arg));
	uint32_t appid = sceLncUtilGetAppId("ITEM00002");
    //Launch Daemon with silent    
    if ((appid & ~0xFFFFFF) != 0x60000000){
        restart_nessary = true;
		logshit("Restart needed for Daemon!");
	}
    else
       logshit("Found Daemon AppId: %x, No restart needed\n", appid);
}

int main(int argc, char* argv[])
{
	init_itemzGL_modules();

	int libjb = -1;

	sceSystemServiceHideSplashScreen();
	sceCommonDialogInitialize();
    sceMsgDialogInitialize();


	if (!LoadLangs(PS4GetLang())) {
		if (!LoadLangs(0x01)) {
			msgok("Failed to load Backup Lang...\nThe App is unable to continue"); 
			goto exit_no_lang;
		}
		else
			logshit("Loaded the backup, lang %i failed to load", PS4GetLang());
	}

    libjb = sceKernelLoadStartModule("/app0/Media/jb.prx", 0, NULL, 0, 0, 0);
    int ret = sceKernelDlsym(libjb, "jbc_run_as_root", (void**)&jbc_run_as_root);
    if (ret >= 0){
        logshit("jbc_run_as_root resolved from PRX\n");
        if (!jbc_run_as_root){
             msgok("%s: %x\n", getLangSTR(FATAL_JB),ret); 
			 goto err;
		}
		else{
			jbc_run_as_root(loader_rooted, NULL, CWD_ROOT);
			logshit("jbc_run_as_root() succesful\n");
		}
    }
	else{
        msgok("%s: %x\n", getLangSTR(FATAL_JB),ret); goto err;
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


	logshit("[Itemz-loader:%s:%i] ----- Launching() ---\n", __FUNCTION__, __LINE__);
	orbisPadFinish();
	if (sceSystemServiceLoadExec("/app0/ItemzCore.self", 0) == 0)
		logshit("[Itemz-loader:%s:%i] ----- Launched (shouldnt see) ---\n", __FUNCTION__, __LINE__);

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
