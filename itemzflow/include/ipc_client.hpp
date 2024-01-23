#pragma once
#ifndef IPC_HEADER_H
#define IPC_HEADER_H

#include <iostream>
#include <vector>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <log.h>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <string>

enum class IPC_Ret {
    INVALID = -1,
    NO_ERROR = 0,
    OPERATION_FAILED = 1,
    FUSE_IP_ALREADY_SET = 3,
    FUSE_IP_NOT_SET = 4,
    FUSE_FW_NOT_SUPPORTED = 5,
    DEAMON_UPDATING = 100
};

enum class IPC_Commands {
    MACGUFFIN_CMD = 69,
    CMD_SHUTDOWN_DAEMON = 1337,
    CONNECTION_TEST = 1,
    ENABLE_HOME_REDIRECT = 2,
    DISABLE_HOME_REDIRECT = 3,
    GAME_GET_MINS = 4,
    GAME_GET_START_TIME = 5,
    CRITICAL_SUSPEND = 6,
    INSTALL_IF_UPDATE = 7,
    RESTART_FTP_SERVICE = 8,
    FUSE_SET_SESSION_IP = 9,
    FUSE_SET_DEBUG_FLAG = 10,
    FUSE_START_W_PATH = 11,
    RESTART_FUSE_FS = 12,
    DEAMON_UPDATE = 100,
};

constexpr size_t DAEMON_BUFF_MAX = 0x100; // Assuming a buffer max size, you can change this.

struct IPCMessage{
    int magic = 0xDEADBABE; 
    IPC_Commands cmd;
    IPC_Ret error = IPC_Ret::INVALID;
    char msg[DAEMON_BUFF_MAX] = {0};
};


const char IPC_SOC[] = "/system_tmp/IPC_Socket";
static int DaemonSocket = -1;

class IPC_Client{
private:
    mutable std::mutex socketMutex;

public:

// Socket Management
int OpenConnection(const char* path) {
    sockaddr_un server;
    int soc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (soc == -1) {
        log_error("Failed to create socket");
        return -1;
    }
    server.sun_family = AF_UNIX;
    strncpy(server.sun_path, path, sizeof(server.sun_path)-1);
    if (connect(soc, (struct sockaddr*)&server, SUN_LEN(&server)) == -1) {
        close(soc);
        log_error("Failed to connect to socket");
        return -1;
    }
    return soc;
}

// IPC Functions
bool IPCOpenConnection() {
    DaemonSocket = OpenConnection(IPC_SOC);
    return DaemonSocket >= 0;
}

bool IPCOpenIfNotConnected() {
    if(DaemonSocket >= 0) {
        return true;
    }
    return IPCOpenConnection();
}
int IPCReceiveData(IPCMessage& msg) {
    return recv(DaemonSocket, reinterpret_cast<void*>(&msg), sizeof(msg), MSG_NOSIGNAL);
}

int IPCSendData(const IPCMessage& msg) {
    int ret = send(DaemonSocket, reinterpret_cast<const void*>(&msg), sizeof(msg), MSG_NOSIGNAL);
    if (ret < 0) {
        log_error("IPCSendData failed with: %s", strerror(errno));
    }
    return ret;
}

IPC_Ret IPCCloseConnection() {
    if (DaemonSocket < 0) {
        return IPC_Ret::INVALID;
    }

    close(DaemonSocket);
    DaemonSocket = -1;
    return IPC_Ret::NO_ERROR;
}


IPC_Ret IPCSendCommand(IPC_Commands cmd, std::string& ipc_msg, bool silent = false) {
    std::lock_guard<std::mutex> lock(socketMutex);
    
    IPCMessage msg;
    msg.cmd = cmd;
    // Copy the string content into the destination array
    strncpy(msg.msg, ipc_msg.c_str(), sizeof(msg.msg) - 1);
    // Null-terminate the destination array
    msg.msg[sizeof(msg.msg) - 1] = '\0';

    IPC_Ret error = IPC_Ret::INVALID;
    if (IPCOpenIfNotConnected()) {
        if(!silent)
           log_debug(" Sending IPC Command");

        IPCSendData(msg);

        if(!silent)
            log_debug(" Sent IPC Command %i", static_cast<int>(cmd));
  
        ipc_msg.clear();

        if (cmd == IPC_Commands::DEAMON_UPDATE) {
            log_debug(" Daemon Updating...");
            IPCCloseConnection();
            return IPC_Ret::DEAMON_UPDATING;
        }

        // Get message back from daemon
        uint32_t readSize = IPCReceiveData(msg);
        if (cmd == IPC_Commands::RESTART_FUSE_FS) {
            log_debug("Restarting FUSE");
            IPCCloseConnection();
        }
        if (static_cast<int>(readSize) >= 0) {
            if(!silent)
               log_debug(" Got message with Size: %i from Daemon", readSize);
            // Get IPC Error code
            error = msg.error;
            if (error != IPC_Ret::INVALID) {
                // Modifies the Buffer to exclude the error code
                if(error == IPC_Ret::NO_ERROR) {
                    ipc_msg = msg.msg;
                }
                else {
                    ipc_msg = "Error";
                }
                if(!silent)
                   log_debug("[ItemzDaemon] Daemon IPC Response: %s, code: %s, readSize: %i", msg.msg, error == IPC_Ret::NO_ERROR ? "NO_ERROR" : "Other", readSize);
            } else {
                log_error("[ItemzDaemon] Daemon returned INVAL");
            }
        } else {
            if(!silent)
               log_error("IPCReceiveData failed with: %s", strerror(errno));
            IPCCloseConnection();
        }
    }
    if (cmd == IPC_Commands::CMD_SHUTDOWN_DAEMON) {
        IPCCloseConnection();
    }

    return error;
}

IPC_Ret IPCMountFUSE(std::string ip, std::string path, bool debug_mode) {
    std::string in;
    IPC_Ret error = IPC_Ret::INVALID;
    IPCSendCommand(IPC_Commands::FUSE_SET_SESSION_IP, ip);
    if (debug_mode) {
        if (IPCSendCommand(IPC_Commands::FUSE_SET_DEBUG_FLAG, in) == IPC_Ret::NO_ERROR) {
            log_debug("FUSE debug mode enabled");
        } else {
            log_error("FUSE failed to enable debug mode");
        }
    }

    // Set the mount point
    // Copy the string content into the destination array
    if ((error = IPCSendCommand(IPC_Commands::FUSE_START_W_PATH, path)) == IPC_Ret::NO_ERROR) {
        log_debug("FUSE started");
    } else {
        log_error("FUSE failed to start with error code: %i", static_cast<int>(error));
    }

    return error;
}
// Deleted copy constructor and assignment operator to ensure only one instance
    IPC_Client& operator=(const IPC_Client&) = delete;

    // Static method to access the instance
    static IPC_Client& getInstance() {
        static IPC_Client instance;  // Lazy-loaded instance, guaranteed to be destroyed
        return instance;
    }
// ... (Placeholder for remaining functions)

}; // namespace IPC

#endif // IPC_HEADER_H
