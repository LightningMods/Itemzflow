#include <errno.h>
#include <Header.h>
#include "ini.h"
#include <sys/param.h>
#include <sys/mount.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/_iovec.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>


#define DAEMON_PATH "/system/vsh/app/ITEM00002"
#define  LATEST_DAEMON_VERSION 0x1001

int daemon_ver = 0x1337;
static int daemon_ini(void* user, const char* section, const char* name,
    const char* value, int idc)
{

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("Daemon", "version")) {
        logshit("DVERLL: %s\n", value);
        daemon_ver = atoi(value);
    }

    return 1;
}


/* *********************** IPC SHIT **********************/
#define IPC_SOC "/system_tmp/IPC_Socket"

int DaemonSocket = -1;
bool is_daemon_connected = false;

int OpenConnection(const char* path)
{
    struct sockaddr_un server;
    int soc = socket(AF_UNIX, SOCK_STREAM, 0);
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, path);
    connect(soc, (struct sockaddr*)&server, SUN_LEN(&server));
    return soc;
}

void GetIPCMessageWithoutError(uint8_t* buf, uint32_t sz)
{
    memmove(buf, buf + 1, sz);
}

bool IPCOpenConnection()
{

    DaemonSocket = OpenConnection(IPC_SOC);
    if (DaemonSocket > 0 && DaemonSocket != -1)
        return true;
    else
        return false;
    
}

int IPCReceiveData(uint8_t* buffer, int32_t size)
{
    return recv(DaemonSocket, buffer, size, MSG_NOSIGNAL);
}

int IPCSendData(uint8_t* buffer, int32_t size)
{
    return send(DaemonSocket, buffer, size, MSG_NOSIGNAL);
}

int IPCCloseConnection()
{
    return close(DaemonSocket);
}

int IPCSendCommand(enum IPC_Commands cmd, uint8_t* IPC_BUFFER) {

    int error = INVALID;

    if (!is_daemon_connected)
           is_daemon_connected = IPCOpenConnection();
    

    if (is_daemon_connected)
    {     

        logshit(" IPC Connected via Domain Socket\n");

        IPC_BUFFER[0] = cmd; // First byte is always command

        logshit(" Sending IPC Command\n");
        IPCSendData(IPC_BUFFER, 100);
        logshit(" Sent IPC Command %i\n", cmd);

        memset(IPC_BUFFER, 0, 100);

        if (cmd == DEAMON_UPDATE)
        {
            logshit(" Daemon Updating...\n");
            is_daemon_connected = false;
            return DEAMON_UPDATING;
        }

        //Get message back from daemon
        uint32_t readSize = IPCReceiveData(IPC_BUFFER, 100);
        if (readSize > 0 && readSize != -1)
        {
            logshit(" Got message with Size: %i from Daemon\n", readSize);
            //Get IPC Error code
            error = IPC_BUFFER[0];
            if (error != INVALID)
            {
                // Modifies the Buffer to exclude the error code
                GetIPCMessageWithoutError(IPC_BUFFER, readSize);
                logshit("[ItemzDaemon] Daemon IPC Response: %s, code: %s, readSize: %i\n", IPC_BUFFER, error == NO_ERROR ? "NO_ERROR" : "Other", readSize);
            }
            else
               logshit("[ItemzDaemon] Daemon returned INVAL\n");
        }
        else
            logshit("IPCReceiveData failed with: %s\n", strerror(errno));
    }


    return error;
}
/* *********************** IPC SHIT **********************/
/* *********************** daemon shit **********************/


bool is_daemon_outdated(void)
{
    
    if (!if_exists(DAEMON_PATH"/daemon.ini"))
        return false;
    else
    {
        logshit(" Daemon INI Does exist\n");
        int error = ini_parse(DAEMON_PATH"/daemon.ini", (ini_handler)daemon_ini, NULL);
        if (error) { logshit("Bad config file (first error on line %d)!\n", error); return false; }
        logshit("Daemon Version: %x, Latest Version: %x, Is Outdated?: %s\n", daemon_ver, LATEST_DAEMON_VERSION, daemon_ver == LATEST_DAEMON_VERSION ? "No" : "Yes");
    }

    return (daemon_ver == LATEST_DAEMON_VERSION);
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

    logshit("##^  [I] Mounting %s \"%s\" to \"%s\"\n", fstype, device, mountpoint);
    ret = nmount(iov, iovlen, flags);
    if (ret < 0) {
        logshit("##^  [E] Failed: %d (errno: %d).\n", ret, errno);
        goto error;
    }
    else {
        logshit("##^  [I] Success.\n");
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

    char buff[100];
    char IPC_BUFFER[100];
    int fd = -1;
    logshit("Booting Daemon Services\n");

    if (!if_exists(DAEMON_PATH) || !is_daemon_outdated())
    {
        if (!!mountfs("/dev/da0x4.crypt", "/system", "exfatfs", "511", MNT_UPDATE))
        {
            logshit("mounting /system failed with %s.\n", strerror(errno));
            return false;
        }
        else
        {

            logshit("Remount Successful\n");
            //Delete the folder and all its files
            rmtree(DAEMON_PATH);
            mkdir(DAEMON_PATH, 0777);
            mkdir(DAEMON_PATH"/Media", 0777);
            mkdir(DAEMON_PATH"/sce_sys", 0777);

            if (copyFile("/mnt/sandbox/pfsmnt/ITEM00001-app0/Media/jb.prx", DAEMON_PATH"/Media/jb.prx") != -1 && 
            copyFile("/system/vsh/app/NPXS21007/sce_sys/param.sfo", DAEMON_PATH"/sce_sys/param.sfo") != -1)
            {

               if ((fd = open(DAEMON_PATH"/eboot.bin", O_WRONLY | O_CREAT | O_TRUNC, 0777)) > 0 && fd != -1) {
                    write(fd, daemon_eboot, daemon_eboot_size);
                    close(fd);

                    snprintf(&buff[0], 99, "[Daemon]\nversion=%i\n", LATEST_DAEMON_VERSION);
                    fd = open(DAEMON_PATH"/daemon.ini", O_WRONLY | O_CREAT | O_TRUNC, 0777);
                    if (fd >= 0)
                    {
                        write(fd, &buff[0], strlen(&buff[0]));
                        close(fd);
                    }
                    else {
                        logshit("Creating the Daemon ini failed to create: %s\n", strerror(errno));
                        return false;
                    }

                    chmod(DAEMON_PATH"/daemon.ini", 0777);
                    IPCSendCommand(DEAMON_UPDATE, (uint8_t *)&IPC_BUFFER[0]);

                }
                else
                {
                    logshit("Creating the Daemon eboot failed to create: %s\n", strerror(errno));
                    return false;
                }
            }
            else
            {
                logshit("Copying Daemon files failed\n");
                return false;
            }
        }
    }

    uint32_t appid = sceLncUtilGetAppId("ITEM00002");
    //Launch Daemon with silent    
    if ((appid & ~0xFFFFFF) != 0x60000000) {
        appid = Launch_Daemon("ITEM00002");
        logshit("Launched Daemon AppId: %x\n", appid);
    }
    else
       logshit("Found Daemon AppId: %x\n", appid);



    return true;

}
/* *********************** daemon shit **********************/