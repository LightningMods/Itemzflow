#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <orbisPad.h>


#include <MsgDialog.h>
#include "Header.h"

const unsigned char completeVersion[] = {VERSION_MAJOR_INIT,
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
										 '\0'};


void logshit(const char* format, ...)
{
     //Check the length of the format string.
    // If it is too long, return immediately.
    if (strlen(format) > 1000) {
        return;
    }

    // Initialize a buffer to hold the formatted log message.
    char buff[1024];

    // Initialize a va_list to hold the variable arguments passed to the function.
    va_list args;
    va_start(args, format);

    // Format the log message using the format string and the variable arguments.
    vsprintf(buff, format, args);

    // Clean up the va_list.
    va_end(args);

    // Append a newline character to the log message.
    strcat(buff, "\n");

    // Output the log message using sceKernelDebugOutText.
    sceKernelDebugOutText(DGB_CHANNEL_TTYL, buff);

	int fd = sceKernelOpen("/user/app/ITEM00001/logs/loader.log", O_WRONLY | O_CREAT | O_APPEND, 0777);
	if (fd >= 0)
	{
		sceKernelWrite(fd, &buff[0], strlen(&buff[0]));
		sceKernelClose(fd);
	}
}


bool rmtree(const char path[]) {

    struct dirent* de;
    char fname[300];
    DIR* dr = opendir(path);
    if (dr == NULL)
    {
        logshit("No file or directory found: %s", path);
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
                logshit("removing dir: %s Err: %d", fname, ret = unlinkat(dirfd(dr), fname, AT_REMOVEDIR));
                if (ret != 0)
                {
                    rmtree(fname);
                    //logshit("Err: %d", ret = unlinkat(dirfd(dr), fname, AT_REMOVEDIR));
                }
            }
            else
            {
                logshit("Removing file: %s, Err: %d", fname, unlink(fname));
            }
        }
    }
    closedir(dr);

    return true;
}

int copyFile(const char* sourcefile, const char* destfile)
{
	int src = sceKernelOpen(sourcefile, 0x0000, 0);
	if (src > 0)
	{
		int out = sceKernelOpen(destfile, 0x0001 | 0x0200 | 0x0400, 0777);
		if (out > 0)
		{
			size_t bytes;
			char* buffer = (char*)malloc(65536);
			if (buffer != NULL)
			{
				while (0 < (bytes = sceKernelRead(src, buffer, 65536)))
					sceKernelWrite(out, buffer, bytes);
				free(buffer);
			}
			sceKernelClose(out);
		}
		else
           return -1;

		
		sceKernelClose(src);
		return 0;
	}
	else
	{
		logshit("[ELFLOADER] fuxking error");
        logshit("[Itemz-loader:%s:%i] ----- src fd = %i---", __FUNCTION__, __LINE__, src);
		return -1;
	}
}

bool if_exists(const char* path)
{
	int dfd = open(path, O_RDONLY, 0); // try to open dir
	if (dfd < 0) {
		logshit("path %s, errno %s", path, strerror(errno));
		return false;
	}
	else
		close(dfd);


	return true;
}

void init_itemzGL_modules()
{
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_USER_SERVICE);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_NETCTL);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_NET);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_HTTP);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SSL);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SYS_CORE);
	sceSysmoduleLoadModuleInternal(0x80000018);
	sceSysmoduleLoadModuleInternal(0x80000026);  // 0x80000026
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_BGFT);  // 0x80000026;
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_APPINSTUTIL);  // 0x80000026 0x80000037
	sceSysmoduleLoadModuleInternal(0x80000024);  // 0x80000026 0x80000037
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);
}

void (*jbc_run_as_root)(void(*fn)(void* arg), void* arg, int cwd_mode);

void loader_rooted(void *arg){
	if(if_exists("/usb0/recovery.flag") || if_exists("/user/recovery.flag")){

		if(Confirmation_Msg("ItemzLoader Recovery Menu\n\n\nDo you want to Factory reset ItemzFlow??\n\nWarning: ItemzFlow App Stats, Covers, Settings, Updates, Themes and More will be Deleted!") == YES){
			   //rmtree app.xml app.pkg app.pbm.backup  app.pbm  app.json
               logshit("Starting factory reset...");
			   mkdir("/user/app/reset", 0777);
			   copyFile("/user/app/ITEM00001/app.xml", "/user/app/reset/app.xml");
			   copyFile("/user/app/ITEM00001/app.pkg", "/user/app/reset/app.pkg");
			   copyFile("/user/app/ITEM00001/app.pbm.backup", "/user/app/reset/app.pbm.backup");
			   copyFile("/user/app/ITEM00001/app.pbm", "/user/app/reset/app.pbm");
			   copyFile("/user/app/ITEM00001/app.json", "/user/app/reset/app.json");

			   rmtree("/user/app/ITEM00001");
			   mkdir("/user/app/ITEM00001", 0777);

			   copyFile("/user/app/reset/app.xml", "/user/app/ITEM00001/app.xml");
			   copyFile("/user/app/reset/app.pkg", "/user/app/ITEM00001/app.pkg");
			   copyFile("/user/app/reset/app.pbm.backup", "/user/app/ITEM00001/app.pbm.backup");
			   copyFile("/user/app/reset/app.pbm", "/user/app/ITEM00001/app.pbm");
			   copyFile("/user/app/reset/app.json", "/user/app/ITEM00001/app.json");
			   rmtree("/user/app/reset");
			   rmdir("/user/app/reset");

			   logshit("Factory reset done!");
			   unlink("/usb0/recovery.flag");
			   rmdir("/usb0/recovery.flag");
			   unlink("/user/recovery.flag");
			   rmdir("/user/recovery.flag");
		       msgok("ItemzLoader Recovery Menu\n\n\nItemzFlow has been Factory Reset!\n\nPress OK to exit");
			   logshit("Factory reset complete!");
			   sceSystemServiceLoadExec("EXIT", NULL);
			}
			else
			   logshit("ItemzLoader Recovery Canned...");

		    unlink("/usb0/recovery.flag");
			rmdir("/usb0/recovery.flag");
			unlink("/user/recovery.flag");
			rmdir("/user/recovery.flag");
	}

	logshit("After jb");
	mkdir("/user/app/ITEM00001/storedata/", 0777);
	mkdir("/user/app/ITEM00001/logs/", 0777);

	unlink("/user/app/ITEM00001/logs/loader.log");

	logshit("[Itemz-loader:%s:%i] -----  All Internal Modules Loaded  -----", __FUNCTION__, __LINE__);
	logshit("------------------------ Itemz Loader[GL] Compiled Time: %s @ %s EST -------------------------", __DATE__, __TIME__);
	logshit("[Itemz-loader:%s:%i] -----  Itemz-Loader Version: %s  -----", __FUNCTION__, __LINE__, completeVersion);
	logshit("----------------------------------------------- -------------------------");

 	if (if_exists("/usb0/settings.ini"))
		logshit("[Itemz-loader:%s:%i] ----- FOUND USB RECOVERY INI ---", __FUNCTION__, __LINE__);
	else
	{
		if (!if_exists("/user/app/ITEM00001/settings.ini"))
		{
			logshit("[Itemz-loader:%s:%i] ----- APP INI Not Found, Making ini ---", __FUNCTION__, __LINE__);

			char buff[1024];
			memset(&buff[0], 0, 1024);
			snprintf(&buff[0], 1024, "[Settings]\nSort_By=1\nSecure_Boot=1\nDumper_option=0\nFTP_Auto_Start=1\nShow_install_prog=1\nHomeMenu_Redirection=0\nDaemon_on_start=1\ncover_message=1\n");

			int fd = open("/user/app/ITEM00001/settings.ini", O_WRONLY | O_CREAT | O_TRUNC, 0777);
			if (fd >= 0)
			{
				write(fd, &buff[0], strlen(&buff[0]));
				close(fd);
			}
			else
				logshit("Could not make create INI File");

			mkdir("/user/app/ITEM00001/downloads", 0777);

		}
		else
		   logshit("[Itemz-loader:%s:%i] ----- INI FOUND ---", __FUNCTION__, __LINE__);

	}

    logshit("[Itemz-loader:%s:%i] -----  Starting daemon services  -----", __FUNCTION__, __LINE__);
	if(boot_daemon_services())
	   logshit("[Itemz-loader:%s:%i] -----  Daemon services started  -----", __FUNCTION__, __LINE__);
	else
	   logshit("[Itemz-loader:%s:%i] -----  Daemon services failed to start  -----", __FUNCTION__, __LINE__);
	 // wrapper();

}

