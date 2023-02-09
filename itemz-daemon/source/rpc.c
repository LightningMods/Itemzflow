#include "defines.h"
#include <utils.h>
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

uint8_t CONNECT_TEST(struct clientArgs* client, uint8_t** mode, uint32_t* length)
{
	// Re-allocate memory
	*mode = realloc(*mode, sizeof("Connected")+1);

	sprintf((char*)(*mode), "Connected");
	*length = strlen((char*)(*mode));

	return NO_ERROR;
}

void *fuse_session_ip = NULL;
extern bool is_home_redirect_enabled;
extern bool critical_suspend;
// extern bool critical_suspend2;

int sys_kill(pid_t pid, int sig)
{
	return syscall(37, pid, sig);
}

uint8_t get_game_mins(struct clientArgs* client, uint8_t* in, uint32_t* length)
{
	char* input = (char*)(in + 1); // TITLE ID, SKIP THE COMMAND BIT

    //DumpHex((const void*)input, strlen(input)); //HEX Dump IPC Title ID

	int mins = get_game_time_played(input); // get_game_time_played(TITLE_ID)
	if(mins == -1){
		log_error("[ItemzDaemon] Failed to get game time played, tid: %s", input);
		return OPERATION_FAILED;
	}
	else
		log_info("[ItemzDaemon] Game time played for %s: %d", input, mins);

    //Build IPC Response without the ERROR Bit
	*length = strlen(input);
	memset(in, 0, *length);

	*length = 70;
	sprintf((char*)in, "%d", mins);

	return NO_ERROR;
}

uint8_t set_fuse_session_ip(struct clientArgs* client, uint8_t* in, uint32_t* length)
{
	char* input = (char*)(in + 1); // TITLE ID, SKIP THE COMMAND BIT

    //DumpHex((const void*)input, strlen(input)); //HEX Dump IPC Title ID
	if(fuse_session_ip){
		log_error("[FUSE SESSION] session ip already set");
		return FUSE_IP_ALREADY_SET;
	}
	else{
		fuse_session_ip = strdup(input);
		log_info("[FUSE SESSION] session ip set to %s", fuse_session_ip);
	}

	return NO_ERROR;
}


uint8_t get_game_start(struct clientArgs* client, uint8_t* in, uint32_t* length)
{
	char* input = (char*)(in + 1);

    //DumpHex((const void*)input, strlen(input));

    time_t date = get_game_start_date(input);
	if(date == 0){
		log_error("[ItemzDaemon] Failed to get game time played, tid: %s", input);
		return OPERATION_FAILED;
	}
	else
		log_info("[ItemzDaemon] Game Start date for %s: ld", input, date);

	 //rewind pointer
	*length = strlen(input);
	memset(in, 0, *length);

	*length = 70;
	sprintf((char*)in, "%ld", (long)date);
	//log_info("[ItemzDaemon] Game Start date for %s: %ld", input, (long)date);

	return NO_ERROR;
}

int isNumber(char *n) {

  int i = strlen(n);
  int isnum = (i>0);
  while (i-- && isnum) {
    if (!(n[i] >= '0' && n[i] <= '9')) {
      isnum = 0;
    }
  }
  return isnum;
}

void handleIPC(struct clientArgs* client, uint8_t* buffer, uint32_t length)
{
	uint8_t error = NO_ERROR;
	uint8_t *outputBuffer = (uint8_t*)malloc(sizeof(uint8_t));
	uint32_t outputLength = 0;

	uint8_t method = buffer[0];
	switch (method)
	{
	case FUSE_SET_DEBUG_FLAG:
		log_info("[Daemon IPC][client %i] FUSE_SET_DEBUG_FLAG command called", client->cl_nmb);
		fuse_debug_flag = true;
        log_info("[Daemon IPC][client %i] Debug FUSE Flag set", client->cl_nmb);
		break;
	case FUSE_SET_SESSION_IP:
		error = set_fuse_session_ip(client, buffer, &outputLength);
		outputBuffer = realloc(outputBuffer, outputLength);
		memcpy(outputBuffer, buffer, outputLength);
		break;
	/*case MACGUFFIN_CMD:
		log_info("[Daemon IPC][client %i] MACGUFFIN command called", client->cl_nmb);
		log_info(" Starting FUSE Thread...");
        scePthreadCreate(&fuse_thread, NULL, initialize_userland_fuse, NULL, "FUSE_Thread");
		break;*/
	case RESTART_FUSE_FS:
		log_info("[Daemon IPC][client %i] RESTART_FUSE_FS command called", client->cl_nmb);
		log_info(" Restarting FUSE & Daemon...");
		uint8_t* fuse_ret_msg = malloc(1);
	    fuse_ret_msg[0] = NO_ERROR; // First byte is always error byte
	    networkSendData(client->socket, fuse_ret_msg, 1);
		unlink("/system_tmp/IPC_init");
		unlink("/system_tmp/IPC_Socket");
		free(fuse_ret_msg);
		reboot_daemon = true;
        raise(SIGQUIT);
		break; 
	case FUSE_START_W_PATH:
		log_info("[Daemon IPC][client %i] FUSE_START_W_PATH command called", client->cl_nmb);
	    const char* fuse_mountpoint = (const char*)(buffer + 1); // get FUSE mountpoint from the daemon command
		char temp[0x100];
		if(strlen(fuse_mountpoint) < 3){
			log_error("[Daemon IPC][client %i] FUSE_START_W_PATH command called with invalid mountpoint", client->cl_nmb);
			error = OPERATION_FAILED;
			break;
		}
		if(!fuse_session_ip){
			log_error("[Daemon IPC][client %i] FUSE_START_W_PATH command called with no session ip set", client->cl_nmb);
			error = FUSE_IP_NOT_SET;
			break;
		}

		strncpy(&temp[0], fuse_session_ip, 0x100); 
		log_info(" Starting FUSE Thread IP %s & mp %s", fuse_session_ip, fuse_mountpoint);
		if((error = initialize_userland_fuse(fuse_mountpoint)) == NO_ERROR){
			log_info(" FUSE Thread Started!");
			save_fuse_ip(&temp[0]);
		}
		else
			log_error("FUSE Thread Failed to Start with code %i!", error);
		
		break;
	case GAME_GET_START_TIME:
		error = get_game_start(client, buffer, &outputLength);
		outputBuffer = realloc(outputBuffer, outputLength);
		memcpy(outputBuffer, buffer, outputLength);
		break;
	case RESTART_FTP_SERVICE:
	    ShutdownFTP();
		notify("FTP Restarting...");
        StartFTP();
        break;
	case SHUTDOWN_DAEMON:
		log_info("[Daemon IPC][client %i] command SHUTDOWN_DAEMON() called", client->cl_nmb);
		notify("Daemon Shutting Down...");
		uint8_t* shutdown_reply = malloc(1);
	    shutdown_reply[0] = NO_ERROR; // First byte is always error byte
	    networkSendData(client->socket, shutdown_reply, 1);
		unlink("/system_tmp/IPC_init");
		unlink("/system_tmp/IPC_Socket");
		free(shutdown_reply);
		raise(SIGQUIT);
		break;
    case GAME_GET_MINS:
		error = get_game_mins(client, buffer, &outputLength);
		outputBuffer = realloc(outputBuffer, outputLength);
		memcpy(outputBuffer, buffer, outputLength);
		break;
	case CONNECTION_TEST: {
		log_info("[Daemon IPC][client %i] command CONNECTION_TEST() called", client->cl_nmb);
		error = CONNECT_TEST(client, &outputBuffer, &outputLength);
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
		uint8_t* outputBufferFull = malloc(1);
	    outputBufferFull[0] = NO_ERROR; // First byte is always error byte
	    networkSendData(client->socket, outputBufferFull, 1);
		free(outputBufferFull);
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
		//Kill IF with SIGQUIT so it rejails
		DumpHex((const void*)buffer, 10);
		if(isNumber((char*)&buffer[1]))
           sys_kill(atoi((char*)&buffer[1]), SIGQUIT);
		else{
			log_error("[Daemon IPC][client %i] command INSTALL_IF_UPDATE() failed, invalid PID (tried %i)", client->cl_nmb, atoi((char*)&buffer[1]));
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
	  log_info("[Daemon IPC][client %i] command %i called", client->cl_nmb, buffer[0]);
	  error = INVALID;
	  break;
	}
	}
   
	uint8_t* outputBufferFull = malloc(outputLength + 1);

	outputBufferFull[0] = error; // First byte is always error byte

	memcpy(&outputBufferFull[1], outputBuffer, outputLength);

	// Send response
	networkSendData(client->socket, outputBufferFull, outputLength + 1);

	// Free allocated memory
	free(outputBuffer);
	outputBuffer = NULL;
	free(outputBufferFull);
	outputBufferFull = NULL;
}

void* ipc_client(void* args)
{
	struct clientArgs* cArgs = (struct clientArgs*)args;


	log_debug("[Daemon IPC] Thread created, Socket %i", cArgs->socket);


	uint32_t readSize = 0;
	uint8_t buffer[100];
	while ((readSize = networkReceiveData(cArgs->socket, buffer, sizeof buffer)) > 0)
	{
		// Handle buffer
		handleIPC(cArgs, buffer, readSize);
		memset(buffer, 0, sizeof buffer);
		//make sure app still exists
		//if(!if_exists("/user/app/ITEM00001/")){
        //    notify("ItemzFlow Not Installed!, Daemon Exiting...");
        //    raise(SIGQUIT);
        //}
	}


	log_debug("[Daemon IPC][client %i] IPC Connection disconnected, Shutting down ...", cArgs->cl_nmb);


	networkCloseConnection(cArgs->socket);
	scePthreadExit(NULL);
	free(args);
	return NULL;
}
