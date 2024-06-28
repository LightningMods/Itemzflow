#include "defines.h"
#include <utils.hpp>
#include <sys/signal.h>
#include "log.h"
#include <ps4sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <user_mem.h>
#include <ctype.h>
#include <mutex>
#include <vector>
#include <fstream>
#include <net.hpp>
#include <ftp.h>
#include <array>


std::mutex ipc_mutex;

void* fuse_startup(void* arg){
	std::ifstream file("/data/itemzflow_daemon/fuse_ip.txt");
    if (!file.is_open()) {
		log_error("[FUSE SESSION] Failed to open fuse_ip.txt");
        return nullptr;
    }

    std::getline(file, fuse_session_ip);

	if(fuse_session_ip.size() > 3){	  
    	log_info(" Starting FUSE Thread IP %s", fuse_session_ip.c_str());
    	if(initialize_userland_fuse("/hostapp", fuse_session_ip.c_str()) == NO_ERROR){
     		log_info(" FUSE Thread Started!");
      		notify("Connected to Network Share!");
    	}
    	else{
	  		log_error("FUSE Thread Failed to Start!");
     		notify("Failed to connect to Network Share!");
    	}

  	    fuse_session_ip.clear();
    }
	return nullptr;
}

void load_fuse_ip() {
    std::ifstream file("/data/itemzflow_daemon/fuse_ip.txt");
    if (!file.is_open()) {
		log_error("[FUSE SESSION] Failed to open fuse_ip.txt");
        return;
    }

    std::getline(file, fuse_session_ip);

	if(fuse_session_ip.size() > 3){	  
    	log_info(" Starting FUSE Thread IP %s", fuse_session_ip.c_str());
    	if(initialize_userland_fuse("/hostapp", fuse_session_ip.c_str()) == NO_ERROR){
     		log_info(" FUSE Thread Started!");
      		notify("Connected to Network Share!");
    	}
    	else{
	  		log_error("FUSE Thread Failed to Start!");
     		notify("Failed to connect to Network Share!");
    	}

  	    fuse_session_ip.clear();
    }
}

IPC_Errors CONNECT_TEST(std::string& mode) {
    mode = "Connected";
    return NO_ERROR;
}

std::string fuse_session_ip;
extern bool is_home_redirect_enabled;
extern bool critical_suspend;
// extern bool critical_suspend2;

int sys_kill(pid_t pid, int sig)
{
	return syscall(37, pid, sig);
}

IPC_Errors get_game_mins(std::string& input) {
    // Extract the title ID, skipping the command bit.
    std::string titleId = input; 

    int mins = get_game_time_played(titleId.c_str()); // Assuming this function still requires a C-style string
    if(mins == -1) {
        log_error("[ItemzDaemon] Failed to get game time played, tid: %s", titleId.c_str());
		input.clear();
        return OPERATION_FAILED;
    } else {
        log_info("[ItemzDaemon] Game time played for %s: %d", titleId.c_str(), mins);
    }

    // Build IPC Response without the ERROR Bit
	input.clear();
    input = std::to_string(mins);

    return NO_ERROR;
}

IPC_Errors set_fuse_session_ip(const std::string& input) {
    // Extract the IP, skipping the command bit.
    std::string ip = input;

    if(fuse_session_ip.size() > 6) {
        log_error("[FUSE SESSION] session ip already set");
        return FUSE_IP_ALREADY_SET;
    } else {
        fuse_session_ip = ip;
        log_info("[FUSE SESSION] session ip set to %s", fuse_session_ip.c_str());
    }

    return NO_ERROR;
}

IPC_Errors get_game_start(std::string& input) {
    // Extract the game title ID, skipping the command bit.
    std::string titleId = input;

    time_t date = get_game_start_date(titleId.c_str());
    if (date == 0) {
        log_error("[ItemzDaemon] Failed to get game start date, tid: %s", titleId.c_str());
        return OPERATION_FAILED;
    } else {
        log_info("[ItemzDaemon] Game Start date for %s: %ld", titleId.c_str(), date);
    }

    input.clear();
    input = std::to_string(static_cast<long>(date));
    return NO_ERROR;
}

bool isNumber(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isdigit(c); });
}

void handleIPC(struct clientArgs* client, std::string &inputStr, cmd command) {
	std::lock_guard<std::mutex> lock(ipc_mutex);
	IPCMessage outputMessage;
	outputMessage.cmd = command;

	int error = NO_ERROR;
	switch (command)
	{
	case FUSE_SET_DEBUG_FLAG:
		log_info("[Daemon IPC][client %i] FUSE_SET_DEBUG_FLAG command called", client->cl_nmb);
		fuse_debug_flag = true;
        log_info("[Daemon IPC][client %i] Debug FUSE Flag set", client->cl_nmb);
		break;
	case FUSE_SET_SESSION_IP:
		error = set_fuse_session_ip(inputStr);
		break;
	/*case MACGUFFIN_CMD:
		log_info("[Daemon IPC][client %i] MACGUFFIN command called", client->cl_nmb);
		log_info(" Starting FUSE Thread...");
        scePthreadCreate(&fuse_thread, NULL, initialize_userland_fuse, NULL, "FUSE_Thread");
		break;*/
	case RESTART_FUSE_FS:
		log_info("[Daemon IPC][client %i] RESTART_FUSE_FS command called", client->cl_nmb);
		log_info(" Restarting FUSE & Daemon...");
        networkSendData(client->socket, reinterpret_cast<void*>(&outputMessage), sizeof(outputMessage));
		unlink("/system_tmp/IPC_init");
		unlink("/system_tmp/IPC_Socket");
		reboot_daemon = true;
        raise(SIGQUIT);
		break; 
	case FUSE_START_W_PATH:{

		 log_info("[Daemon IPC][client %i] FUSE_START_W_PATH command called", client->cl_nmb);
         if(inputStr.size() < 3) {
            log_error("[Daemon IPC][client %i] FUSE_START_W_PATH command called with invalid mountpoint", client->cl_nmb);
            error = OPERATION_FAILED;
            break;
         }

   		if(fuse_session_ip.size() < 6) {
        	log_error("[Daemon IPC][client %i] FUSE_START_W_PATH command called with no session ip set sz: %zu", client->cl_nmb, fuse_session_ip.size());
       		error = FUSE_IP_NOT_SET;
        	break;
   		}

   		log_info(" Starting FUSE Thread IP %s & mp %s", fuse_session_ip.c_str(), inputStr.c_str());
        if((error = initialize_userland_fuse(inputStr.c_str(), fuse_session_ip.c_str())) == NO_ERROR) {
        	log_info(" FUSE Thread Started!");
        	save_fuse_ip(fuse_session_ip.c_str());
   		} else {
        	log_error("FUSE Thread Failed to Start with code %i!", static_cast<int>(error));
    	}

   		fuse_session_ip.clear();  // Clear the session IP string 
    	break;
    }
	case GAME_GET_START_TIME:
		error = get_game_start(inputStr);
		break;
	case RESTART_FTP_SERVICE:
	    ShutdownFTP();
		notify("FTP Restarting...");
        StartFTP();
        break;
	case SHUTDOWN_DAEMON:
		log_info("[Daemon IPC][client %i] command SHUTDOWN_DAEMON() called", client->cl_nmb);
		//DumpHex(buffer.data(), 10);
		notify("Daemon Shutting Down...");
	    networkSendData(client->socket, reinterpret_cast<void*>(&outputMessage), sizeof(outputMessage));
		unlink("/system_tmp/IPC_init");
		unlink("/system_tmp/IPC_Socket");
		raise(SIGQUIT);
		break;
    case GAME_GET_MINS:
		error = get_game_mins(inputStr);
		break;
	case CONNECTION_TEST: {
		log_info("[Daemon IPC][client %i] command CONNECTION_TEST() called", client->cl_nmb);
		error = CONNECT_TEST(inputStr);
		break;
	}
	case DISABLE_HOME_REDIRECT: {
		log_info("[Daemon IPC][client %i] command DISABLE_HOME_REDIRECT() called", client->cl_nmb);
		is_home_redirect_enabled = false;
		error = NO_ERROR;
		break;
	}
	case  ENABLE_HOME_REDIRECT: {
		log_info("[Daemon IPC][client %i] command ENABLE_HOME_REDIRECT() called", client->cl_nmb);
		is_home_redirect_enabled = true;
		error = NO_ERROR;
		break;
	}
	case DEAMON_UPDATE: {
		log_info("[Daemon IPC][client %i] command DEAMON_UPDATE() called", client->cl_nmb);
		log_info("[Daemon IPC][client %i] Reloading Daemon ...", client->cl_nmb);
		log_info("Daemon Rejailed, Rebooting Daemon ...");
	    networkSendData(client->socket, reinterpret_cast<void*>(&outputMessage), sizeof(outputMessage));
		reboot_daemon = true;
		unlink("/system_tmp/IPC_init");
		unlink("/system_tmp/IPC_Socket");
        raise(SIGQUIT);
		break;
	}
	case CRITICAL_SUSPEND: {
		log_info("[Daemon IPC][client %i] command CRITICAL_SUSPEND() called", client->cl_nmb);
		critical_suspend = true;
		log_info("[Daemon] Critical Suspend activated");
		error = NO_ERROR;
		break;
	}
    case INSTALL_IF_UPDATE:{
		 // Terminate IF with SIGQUIT to re-jail it
    	//DumpHex(buffer.data(), 10);

    	if (isNumber(inputStr)) {
       		// int pid = std::stoi(inputStr);
        	 //sys_kill(pid, SIGQUIT);
   		} else {
        	log_error("[Daemon IPC][client %i] command INSTALL_IF_UPDATE() failed, invalid PID", client->cl_nmb);
        	error = OPERATION_FAILED;
        	break;
   		}

    	log_info("[Daemon IPC][client %i] command INSTALL_IF_UPDATE() called", client->cl_nmb);
		mkdir("/user/update_tmp", 0777);
		mkdir("/user/update_tmp/covers", 0777);
		mkdir("/user/update_tmp/custom_app_names", 0777);
        copyRegFile("/user/app/ITEM00001/settings.ini", "/user/update_tmp/settings.ini");
		copy_dir("/user/app/ITEM00001/covers", "/user/update_tmp/covers");
		copy_dir("/user/app/ITEM00001/custom_app_names", "/user/update_tmp/custom_app_names");

		//copy_dir("/user/update", "/user/update_tmp");
		long pkg_size = 0;
		if(if_exists("/user/app/if_update.pkg") && (pkg_size = CalcAppsize("/user/app/if_update.pkg")) > 0){
           log_info("[Daemon] Update Return 0x%X | sz %i", pkginstall("/user/app/if_update.pkg", true), pkg_size);
		   log_debug("[Daemon] Copying files back ...");

           copy_dir("/user/update_tmp/covers", "/user/app/ITEM00001/covers");
		   copy_dir("/user/update_tmp/custom_app_names", "/user/app/ITEM00001/custom_app_names");
		   copyRegFile("/user/update_tmp/settings.ini", "/user/app/ITEM00001/settings.ini");
		   unlink("/user/app/if_update.pkg");
		   unlink("/mnt/usb0/if_update.pkg");
		   unlink("/mnt/usb1/if_update.pkg");
		   unlink("/mnt/usb2/if_update.pkg");
		   unlink("/user/app/if_update.pkg.sig");
		   rmdir("/user/update_tmp");
		   error = NO_ERROR;
		}
		else{
			log_info("[Daemon] No Update Found or 0 bytes Update");
		    error = INVALID;
		}

		break;
	}	
	default:{//
	  log_info("[Daemon IPC][client %i] command %i called", client->cl_nmb, command);
	  error = INVALID;
	  break;
	}
	}

	outputMessage.error = (IPC_Errors)error;
   
    if (!inputStr.empty()) {
		strncpy(outputMessage.msg, inputStr.c_str(), sizeof(outputMessage.msg) - 1);
        // Null-terminate the destination array
        outputMessage.msg[sizeof(outputMessage.msg) - 1] = '\0';
    } 

	networkSendData(client->socket, reinterpret_cast<void*>(&outputMessage), sizeof(outputMessage));
}

void* ipc_client(void* args)
{
    struct clientArgs* client = (struct clientArgs*)args;
    log_debug("[Daemon IPC] Thread created for Socket %i", client->socket);

    uint32_t readSize = 0;
    IPCMessage ipcMessage; // Create an IPCMessage struct to store received data

    while ((readSize = networkReceiveData(client->socket, reinterpret_cast<void*>(&ipcMessage), sizeof(ipcMessage))) > 0) {
        if (ipcMessage.magic == 0xDEADBABE) {
            // Handle IPCMessage
            std::string message = ipcMessage.msg; // Retrieve the string message
            handleIPC(client, message, ipcMessage.cmd);
        } else {
            log_error("[Daemon IPC][client %i] Invalid magic number", client->cl_nmb);
        }
    }

    log_debug("[Daemon IPC][client %i] IPC Connection disconnected, Shutting down ...", client->cl_nmb);
    networkCloseConnection(client->socket);
    delete client;
    scePthreadExit(NULL);

    return NULL;
}