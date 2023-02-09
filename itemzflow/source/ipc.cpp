#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "ini.h"
#include <md5.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

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
    if (DaemonSocket >= 0)
        return true;
    else
        return false;   
}

bool IPCOpenIfNotConnected()
{
    if (!is_daemon_connected)
         is_daemon_connected = IPCOpenConnection();

    return is_daemon_connected;
}

int IPCReceiveData(uint8_t* buffer, int32_t size)
{
    IPCOpenIfNotConnected();
    return recv(DaemonSocket, buffer, size, MSG_NOSIGNAL);
}

int IPCSendData(uint8_t* buffer, int32_t size)
{
    IPCOpenIfNotConnected();
    return send(DaemonSocket, buffer, size, MSG_NOSIGNAL);
}

int IPCCloseConnection()
{
    if(!is_daemon_connected || DaemonSocket < 0)
        return INVALID;

    is_daemon_connected = false;
    return close(DaemonSocket), DaemonSocket = -1;
}

IPC_Ret IPCSendCommand(enum IPC_Commands cmd, uint8_t* IPC_BUFFER) {

    IPC_Ret error = INVALID;
    if (IPCOpenIfNotConnected())
    {     
        log_debug(" IPC Connected via Domain Socket");
        IPC_BUFFER[0] = cmd; // First byte is always command
        log_debug(" Sending IPC Command");
        IPCSendData(IPC_BUFFER, DAEMON_BUFF_MAX);
        log_debug(" Sent IPC Command %i", cmd);

        memset(IPC_BUFFER, 0, DAEMON_BUFF_MAX);

        if (cmd == DEAMON_UPDATE)
        {
            log_debug(" Daemon Updating...");
            IPCCloseConnection();
            return DEAMON_UPDATING;
        }

        //Get message back from daemon
        uint32_t readSize = IPCReceiveData(IPC_BUFFER, DAEMON_BUFF_MAX);
        if(cmd == RESTART_FUSE_FS){
            log_debug("Restarting FUSE");
            IPCCloseConnection();
        }
        if ((int)readSize >= 0)
        {
            log_debug(" Got message with Size: %i from Daemon", readSize);
            //Get IPC Error code
            error = (IPC_Ret)IPC_BUFFER[0];
            if (error != INVALID)
            {
                // Modifies the Buffer to exclude the error code
                GetIPCMessageWithoutError(IPC_BUFFER, readSize);
                log_debug("[ItemzDaemon] Daemon IPC Response: %s, code: %s, readSize: %i", IPC_BUFFER, error == NO_ERROR ? "NO_ERROR" : "Other", readSize);
            }
            else
               log_error("[ItemzDaemon] Daemon returned INVAL");
        }
        else{
            log_error("IPCReceiveData failed with: %s", strerror(errno));
            IPCCloseConnection();
        }
    }
    if(cmd == CMD_SHUTDOWN_DAEMON)
        IPCCloseConnection();

    return error;
}

IPC_Ret IPCMountFUSE(const char* ip, const char* path, bool debug_mode)
{
    char ipc_msg[100];
    IPC_Ret error = INVALID;
    memset(&ipc_msg[0], 0, sizeof ipc_msg);
    // Set the IP address of the PC for the FUSE session
    strncpy((char*)&ipc_msg[1], ip, sizeof(ipc_msg) - 2); 
    IPCSendCommand(FUSE_SET_SESSION_IP, (uint8_t*)&ipc_msg[0]);

    if(debug_mode){
      memset(&ipc_msg[0], 0, sizeof ipc_msg);
      if(IPCSendCommand(FUSE_SET_DEBUG_FLAG, (uint8_t*)&ipc_msg[0]) == NO_ERROR){
        log_debug("FUSE debug mode enabled");
      }
      else
        log_error("FUSE failed to enable debug mode");
    }
           
    // set the mount point
    memset(&ipc_msg[0], 0, sizeof ipc_msg);
    strncpy((char*)&ipc_msg[1], path, sizeof(ipc_msg) - 2);

    if((error = IPCSendCommand(FUSE_START_W_PATH, (uint8_t*)&ipc_msg[0])) == NO_ERROR)
        log_debug("FUSE started");
    else
        log_error("FUSE failed to start with error code: %i", error);

    return error;
}