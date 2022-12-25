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

        log_debug(" IPC Connected via Domain Socket");

        IPC_BUFFER[0] = cmd; // First byte is always command

        log_debug(" Sending IPC Command");
        IPCSendData(IPC_BUFFER, DAEMON_BUFF_MAX);
        log_debug(" Sent IPC Command %i", cmd);

        memset(IPC_BUFFER, 0, DAEMON_BUFF_MAX);

        if (cmd == DEAMON_UPDATE)
        {
            log_debug(" Daemon Updating...");
            is_daemon_connected = false;
            return DEAMON_UPDATING;
        }

        //Get message back from daemon
        uint32_t readSize = IPCReceiveData(IPC_BUFFER, DAEMON_BUFF_MAX);
        if (readSize > 0 && readSize != -1)
        {
            log_debug(" Got message with Size: %i from Daemon", readSize);
            //Get IPC Error code
            error = IPC_BUFFER[0];
            if (error != INVALID)
            {
                // Modifies the Buffer to exclude the error code
                GetIPCMessageWithoutError(IPC_BUFFER, readSize);

                log_debug("[ItemzDaemon] Daemon IPC Response: %s, code: %s, readSize: %i", IPC_BUFFER, error == NO_ERROR ? "NO_ERROR" : "Other", readSize);
            }
            else
               log_error("[ItemzDaemon] Daemon returned INVAL");
        }
        else
            log_error("IPCReceiveData failed with: %s", strerror(errno));
    }


    return error;
}