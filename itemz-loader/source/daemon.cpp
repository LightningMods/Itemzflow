#include <errno.h>
#include <Header.h>
#include "ini.h"
#include <sys/param.h>
#include <sys/mount.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/_iovec.h>
#include "md5.h"
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>


#define DAEMON_PATH "/system/vsh/app/ITEM00002"

/* *********************** IPC SHIT **********************/
#include "ipc_client.hpp"
/* *********************** IPC SHIT **********************/
/* *********************** daemon shit **********************/

int MD5_hash_compare(const char* file1, const char* file2)
{
	unsigned char c[MD5_HASH_LENGTH];
	unsigned char c2[MD5_HASH_LENGTH];
	MD5_CTX mdContext, mdContext2;
	int bytes2, bytes = 0;
	unsigned char data2[1024];
	unsigned char data[1024];

	FILE* f1 = fopen(file1, "rb");
	if(!f1) 
       logshit("cannot open: %s", file1);
       return true;
	
	FILE* f2 = fopen(file2, "rb");
	if(!f2){
      logshit("cannot open: %s", file2);
	  fclose(f1);
      return true;
	}

	MD5_Init(&mdContext);
	while ((bytes = fread(data, 1, 1024, f1)) != 0)
		MD5_Update(&mdContext, data, bytes);
	MD5_Final(c, &mdContext);

	MD5_Init(&mdContext2);
	while ((bytes2 = fread(data2, 1, 1024, f2)) != 0)
		MD5_Update(&mdContext2, data2, bytes2);
	MD5_Final(c2, &mdContext2);

	for (int i = 0; i < 16; i++)
	{
		if (c[i] != c2[i]){
	       fclose(f1);
	       fclose(f2);
		   return true;
		}
	}

	fclose(f1);
	fclose(f2);

	return false;
}

bool is_daemon_outdated(void)
{
    bool res = true;
    if (if_exists(DAEMON_PATH"/eboot.bin"))
    {
        res = MD5_hash_compare(DAEMON_PATH"/eboot.bin", "/mnt/sandbox/pfsmnt/ITEM00001-app0/daemon/daemon.self");
        logshit("Daemon Is Outdated?: %s", res ? "Yes" : "No");
    }
    
    return res;
}

void build_iovec(struct iovec** iov, int* iovlen, const char* name, const void* val, size_t len) {
    int i;

    if (*iovlen < 0)
        return;

    i = *iovlen;
    *iov = (struct iovec*)realloc((void*)(*iov), sizeof(struct iovec) * (i + 2));
    if (*iov == NULL) {
        *iovlen = -1;
        return;
    }

    (*iov)[i].iov_base = strdup(name);
    (*iov)[i].iov_len = strlen(name) + 1;
    ++i;

    (*iov)[i].iov_base = (void*)val;
    if (len == (size_t)-1) {
        if (val != NULL)
            len = strlen((const char*)val) + 1;
        else
            len = 0;
    }
    (*iov)[i].iov_len = (int)len;

    *iovlen = ++i;
}


int mountfs(const char* device, const char* mountpoint, const char* fstype, const char* mode, uint64_t flags)
{
    struct iovec* iov = NULL;
    int iovlen = 0;
    int ret;

    build_iovec(&iov, &iovlen, "fstype", fstype, -1);
    build_iovec(&iov, &iovlen, "fspath", mountpoint, -1);
    build_iovec(&iov, &iovlen, "from", device, -1);
    build_iovec(&iov, &iovlen, "large", "yes", -1);
    build_iovec(&iov, &iovlen, "timezone", "static", -1);
    build_iovec(&iov, &iovlen, "async", "", -1);
    build_iovec(&iov, &iovlen, "ignoreacl", "", -1);

    if (mode) {
        build_iovec(&iov, &iovlen, "dirmask", mode, -1);
        build_iovec(&iov, &iovlen, "mask", mode, -1);
    }

    logshit("##^  [I] Mounting %s \"%s\" to \"%s\"", fstype, device, mountpoint);
    ret = nmount(iov, iovlen, flags);
    if (ret < 0) {
        logshit("##^  [E] Failed: %d (errno: %d).", ret, errno);
        goto error;
    }
    else {
        logshit("##^  [I] Success.");
    }

error:
    return ret;
}

uint32_t Launch_Daemon(const char* TITLE_ID) {
    uint32_t userId = -1;

    int libcmi = sceKernelLoadStartModule("/system/common/lib/libSceSystemService.sprx", 0, NULL, 0, 0, 0);
    if (libcmi > 0)
    {

        LncAppParam param;
        param.sz = sizeof(LncAppParam);
        param.user_id = userId;
        param.app_opt = 0;
        param.crash_report = 0;
        param.check_flag = SkipSystemUpdateCheck;

        return sceLncUtilLaunchApp(TITLE_ID, NULL, &param);
    }
  
    return 0;
}

bool boot_daemon_services()
{
    std::string ipc_msg;
    logshit("Booting Daemon Services");
    IPC_Client& ipc = IPC_Client::getInstance();

    if (!if_exists(DAEMON_PATH) || is_daemon_outdated())
    {
        if (!!mountfs("/dev/da0x4.crypt", "/system", "exfatfs", "511", MNT_UPDATE))
        {
            logshit("mounting /system failed with %s.", strerror(errno));
            return false;
        }
        else
        {

            logshit("Remount Successful");
            //Delete the folder and all its files
            rmtree(DAEMON_PATH);
            mkdir(DAEMON_PATH, 0777);
            mkdir(DAEMON_PATH"/Media", 0777);
            mkdir(DAEMON_PATH"/sce_sys", 0777);

            if (copyFile("/mnt/sandbox/pfsmnt/ITEM00001-app0/Media/jb.prx", DAEMON_PATH"/Media/jb.prx") != -1 && 
            copyFile("/system/vsh/app/NPXS21007/sce_sys/param.sfo", DAEMON_PATH"/sce_sys/param.sfo") != -1)
            {
                if (copyFile("/mnt/sandbox/pfsmnt/ITEM00001-app0/daemon/daemon.self", DAEMON_PATH"/eboot.bin") == 0) {
                    ipc.IPCSendCommand(IPC_Commands::DEAMON_UPDATE, ipc_msg);
                }
                else
                {
                    logshit("Creating the Daemon eboot failed to create: %s", strerror(errno));
                    return false;
                }
            }
            else
            {
                logshit("Copying Daemon files failed");
                return false;
            }
        }
    }

    uint32_t appid = sceLncUtilGetAppId("ITEM00002");
    //Launch Daemon with silent    
    if ((appid & ~0xFFFFFF) != 0x60000000) {
        appid = Launch_Daemon("ITEM00002");
        logshit("Launched Daemon AppId: %x", appid);
    }
    else
       logshit("Found Daemon AppId: %x", appid);



    return true;

}
/* *********************** daemon shit **********************/