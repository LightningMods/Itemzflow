/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#ifndef FTPS4_H
#define FTPS4_H

#include "defines.h"
#include <netinet/in.h>
#include <orbis/libkernel.h>

typedef void (*ftps4_log_cb_t)(const char*);
#define PATH_MAX 1024
/* Returns PS4's IP and FTP port. 0 on success */
bool ftps4_init(const char* ip, unsigned short int port);
void ftps4_set_file_buf_size(unsigned int size);

/* Extended functionality */

#define FTPS4_EOL "\r\n"

typedef enum {
	FTP_DATA_CONNECTION_NONE,
	FTP_DATA_CONNECTION_ACTIVE,
	FTP_DATA_CONNECTION_PASSIVE,
} DataConnectionType;

typedef struct ftps4_client_info {
	/* Client number */
	int num;
	/* Thread UID */
	ScePthread thid;
	/* Control connection socket FD */
	int ctrl_sockfd;
	/* Data connection attributes */
	int data_sockfd;
	DataConnectionType data_con_type;
	struct sockaddr_in data_sockaddr;
	/* PASV mode client socket */
	struct sockaddr_in pasv_sockaddr;
	int pasv_sockfd;
	/* Remote client net info */
	struct sockaddr_in addr;
	/* Receive buffer attributes */
	int n_recv;
	char recv_buffer[512];
	/* Points to the character after the first space */
	const char* recv_cmd_args;
	/* Current working directory */
	char cur_path[PATH_MAX];
	/* Rename path */
	char rename_path[PATH_MAX];
	/* Client list */
	struct ftps4_client_info* next;
	struct ftps4_client_info* prev;
	/* Offset for transfer resume */
	unsigned int restore_point;
} ftps4_client_info_t;


typedef void (*cmd_dispatch_func)(ftps4_client_info_t* client); // Command handler

int ftps4_ext_add_command(const char* cmd, cmd_dispatch_func func);
int ftps4_ext_del_command(const char* cmd);
void ftps4_ext_client_send_ctrl_msg(ftps4_client_info_t* client, const char* msg);
void ftps4_ext_client_send_data_msg(ftps4_client_info_t* client, const char* str);
void ftps4_gen_ftp_fullpath(ftps4_client_info_t* client, char* path, size_t path_size);

bool StartFTP();
void ShutdownFTP();
void check_ftp_addr_change();
#endif