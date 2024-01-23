#include "ftp.h"
#include "defines.h"
#include <sys/socket.h>
#include "net.hpp"
#include <netinet/in.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <vector>
#include "log.h"
#include <libkernel.h>
#include "utils.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat"

#define UNUSED(x) (void)(x)
#define NET_INIT_SIZE (64 * 1024)
#define DEFAULT_FILE_BUF_SIZE (4 * 1024 * 1024)

#define FTP_DEFAULT_PATH   "/"

#define MAX_COMMANDS 32
char ip_address[20];


typedef struct SceNetEtherAddr {
    uint8_t data[6];
} SceNetEtherAddr;

typedef union SceNetCtlInfo {
    uint32_t device;
    SceNetEtherAddr ether_addr;
    uint32_t mtu;
    uint32_t link;
    SceNetEtherAddr bssid;
    char ssid[33];
    uint32_t wifi_security;
    int32_t rssi_dbm;
    uint8_t rssi_percentage;
    uint8_t channel;
    uint32_t ip_config;
    char dhcp_hostname[256];
    char pppoe_auth_name[128];
    char ip_address[16];
    char netmask[16];
    char default_route[16];
    char primary_dns[16];
    char secondary_dns[16];
    uint32_t http_proxy_config;
    char http_proxy_server[256];
    uint16_t http_proxy_port;
} SceNetCtlInfo;

static struct {
	const char* cmd;
	cmd_dispatch_func func;
	int valid;
} command_dispatchers[MAX_COMMANDS];

static int ftp_initialized = 0;
static unsigned int file_buf_size;
static struct in_addr ps4_addr;
static unsigned short int ps4_port;
static ScePthread server_thid;
static int server_sockfd;
static int number_clients = 0;
static ftps4_client_info_t* client_list = NULL;
static ScePthreadMutex client_list_mtx;

#define client_send_ctrl_msg(cl, str) \
	sceNetSend(cl->ctrl_sockfd, str, strlen(str), 0)

static void cmd_installpkg_func(ftps4_client_info_t *client)
{
	int ret = -1;
    client_send_ctrl_msg(client, "Installing PKG ..." FTPS4_EOL);
	log_info("[FTP] Installing PKG ...");
    if((ret = pkginstall("/user/app/temp.pkg", false)) < 0){
		notify("[FTP] Failed to install PKG see log for error message");
		log_info("[FTP] Failed to install PKG with code: %i", ret);
		client_send_ctrl_msg(client, "500 installpkg command failed!" FTPS4_EOL);

	}
	else{
		log_info("[FTP] PKG installing");
    	notify("[FTP] PKG is now installing ...");
		client_send_ctrl_msg(client, "200 installpkg command successful!" FTPS4_EOL);

	}
}
static inline void client_send_data_msg(ftps4_client_info_t* client, const char* str)
{
	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		sceNetSend(client->data_sockfd, str, strlen(str), 0);
	}
	else {
		sceNetSend(client->pasv_sockfd, str, strlen(str), 0);
	}
}

static inline int client_recv_data_raw(ftps4_client_info_t* client, void* buf, unsigned int len)
{
	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		return sceNetRecv(client->data_sockfd, buf, len, 0);
	}
	else {
		return sceNetRecv(client->pasv_sockfd, buf, len, 0);
	}
}

static inline void client_send_data_raw(ftps4_client_info_t* client, const void* buf, unsigned int len)
{
	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		sceNetSend(client->data_sockfd, buf, len, 0);
	}
	else {
		sceNetSend(client->pasv_sockfd, buf, len, 0);
	}
}

static int file_exists(const char* path)
{
	struct stat s;
	return (stat(path, &s) >= 0);
}

static void cmd_NOOP_func(ftps4_client_info_t* client)
{
	client_send_ctrl_msg(client, "200 No operation ;)" FTPS4_EOL);
}

static void cmd_USER_func(ftps4_client_info_t* client)
{
	client_send_ctrl_msg(client, "331 Username OK, need password b0ss." FTPS4_EOL);
}

static void cmd_PASS_func(ftps4_client_info_t* client)
{
	client_send_ctrl_msg(client, "230 User logged in!" FTPS4_EOL);
}

static void cmd_QUIT_func(ftps4_client_info_t* client)
{
	client_send_ctrl_msg(client, "221 Goodbye senpai :'(" FTPS4_EOL);
}

static void cmd_SYST_func(ftps4_client_info_t* client)
{
	client_send_ctrl_msg(client, "215 UNIX Type: L8" FTPS4_EOL);
}

static void cmd_PASV_func(ftps4_client_info_t* client)
{
	int ret;
	UNUSED(ret);

	char cmd[512];
	unsigned int namelen;
	struct sockaddr_in picked;

	/* Create data mode socket name */
	char data_socket_name[64];
	sprintf(data_socket_name, "FTPS4_client_%i_data_socket",
		client->num);

	/* Create the data socket */
	client->data_sockfd = sceNetSocket(data_socket_name,
		AF_INET,
		SOCK_STREAM,
		0);

	log_debug("[FTP Module] PASV data socket fd: %d", client->data_sockfd);

	/* Fill the data socket address */
	client->data_sockaddr.sin_len = sizeof(client->data_sockaddr);
	client->data_sockaddr.sin_family = AF_INET;
	client->data_sockaddr.sin_addr.s_addr = sceNetHtonl(0);
	/* Let the PS4 choose a port */
	client->data_sockaddr.sin_port = sceNetHtons(0);

	/* Bind the data socket address to the data socket */
	ret = sceNetBind(client->data_sockfd,
		(struct sockaddr*)&client->data_sockaddr,
		sizeof(client->data_sockaddr));
	log_debug("[FTP Module] sceNetBind(): 0x%08X", ret);

	/* Start listening */
	ret = sceNetListen(client->data_sockfd, 128);
	log_debug("[FTP Module] sceNetListen(): 0x%08X", ret);

	/* Get the port that the PS4 has chosen */
	namelen = sizeof(picked);
	sceNetGetsockname(client->data_sockfd, (struct sockaddr*)&picked,
		&namelen);

	log_debug("[FTP Module] PASV mode port: 0x%04X", picked.sin_port);

	/* Build the command */
	sprintf(cmd, "227 Entering Passive Mode (%hhu,%hhu,%hhu,%hhu,%hhu,%hhu)" FTPS4_EOL,
		(ps4_addr.s_addr >> 0) & 0xFF,
		(ps4_addr.s_addr >> 8) & 0xFF,
		(ps4_addr.s_addr >> 16) & 0xFF,
		(ps4_addr.s_addr >> 24) & 0xFF,
		(picked.sin_port >> 0) & 0xFF,
		(picked.sin_port >> 8) & 0xFF);

	client_send_ctrl_msg(client, cmd);

	/* Set the data connection type to passive! */
	client->data_con_type = FTP_DATA_CONNECTION_PASSIVE;
}

static void cmd_PORT_func(ftps4_client_info_t* client)
{
	unsigned char data_ip[4];
	unsigned char porthi, portlo;
	unsigned short data_port;
	char ip_str[16];
	struct in_addr data_addr;
	int n;

	if (!client->recv_cmd_args) {
		client_send_ctrl_msg(client, "500 Syntax error, command unrecognized." FTPS4_EOL);
		return;
	}

	n = sscanf(client->recv_cmd_args, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
		&data_ip[0], &data_ip[1], &data_ip[2], &data_ip[3],
		&porthi, &portlo);
	if (n != 6) {
		client_send_ctrl_msg(client, "500 Syntax error, command unrecognized." FTPS4_EOL);
		return;
	}

	data_port = portlo + porthi * 256;

	/* Convert to an X.X.X.X IP string */
	sprintf(ip_str, "%d.%d.%d.%d",
		data_ip[0], data_ip[1], data_ip[2], data_ip[3]);

	/* Convert the IP to a struct in_addr */
	sceNetInetPton(AF_INET, ip_str, &data_addr);

	log_debug("[FTP Module] PORT connection to client's IP: %s Port: %d", ip_str, data_port);

	/* Create data mode socket name */
	char data_socket_name[64];
	sprintf(data_socket_name, "FTPS4_client_%i_data_socket",
		client->num);

	/* Create data mode socket */
	client->data_sockfd = sceNetSocket(data_socket_name,
		AF_INET,
		SOCK_STREAM,
		0);

	log_debug("[FTP Module] Client %i data socket fd: %d", client->num,
		client->data_sockfd);

	/* Prepare socket address for the data connection */
	client->data_sockaddr.sin_len = sizeof(client->data_sockaddr);
	client->data_sockaddr.sin_family = AF_INET;
	client->data_sockaddr.sin_addr = data_addr;
	client->data_sockaddr.sin_port = sceNetHtons(data_port);

	/* Set the data connection type to active! */
	client->data_con_type = FTP_DATA_CONNECTION_ACTIVE;

	client_send_ctrl_msg(client, "200 PORT command successful!" FTPS4_EOL);
}

static void client_open_data_connection(ftps4_client_info_t* client)
{
	int ret;
	UNUSED(ret);

	unsigned int addrlen;

	if (client->data_con_type == FTP_DATA_CONNECTION_ACTIVE) {
		/* Connect to the client using the data socket */
		ret = sceNetConnect(client->data_sockfd,
			(struct sockaddr*)&client->data_sockaddr,
			sizeof(client->data_sockaddr));

		log_debug("[FTP Module] sceNetConnect(): 0x%08X", ret);
	}
	else {
		/* Listen to the client using the data socket */
		addrlen = sizeof(client->pasv_sockaddr);
		client->pasv_sockfd = sceNetAccept(client->data_sockfd,
			(struct sockaddr*)&client->pasv_sockaddr,
			&addrlen);
		log_debug("[FTP Module] PASV client fd: 0x%08X", client->pasv_sockfd);
	}
}

static void client_close_data_connection(ftps4_client_info_t* client)
{
	sceNetSocketClose(client->data_sockfd);
	/* In passive mode we have to close the client pasv socket too */
	if (client->data_con_type == FTP_DATA_CONNECTION_PASSIVE) {
		sceNetSocketClose(client->pasv_sockfd);
	}
	client->data_con_type = FTP_DATA_CONNECTION_NONE;
}

static char file_type_char(mode_t mode)
{
	return S_ISBLK(mode) ? 'b' :
		S_ISCHR(mode) ? 'c' :
		S_ISREG(mode) ? '-' :
		S_ISDIR(mode) ? 'd' :
		S_ISFIFO(mode) ? 'p' :
		S_ISSOCK(mode) ? 's' :
		S_ISLNK(mode) ? 'l' : ' ';
}

static int gen_list_format(char* out, int n, mode_t file_mode, unsigned long long file_size,
	const struct tm file_tm, const char* file_name, const char* link_name, const struct tm cur_tm)
{
	static const char num_to_month[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	char yt[6];

	if (cur_tm.tm_year == file_tm.tm_year) {
		snprintf(yt, sizeof(yt), "%02d:%02d", file_tm.tm_hour, file_tm.tm_min);
	}
	else {
		snprintf(yt, sizeof(yt), "%04d", 1900 + file_tm.tm_year);
	}

#define LIST_FMT "%c%c%c%c%c%c%c%c%c%c 1 ps4 ps4 %llu %s %2d %s %s"
#define LIST_ARGS \
			file_type_char(file_mode), \
			file_mode & 0400 ? 'r' : '-', \
			file_mode & 0200 ? 'w' : '-', \
			file_mode & 0100 ? (S_ISDIR(file_mode) ? 's' : 'x') : (S_ISDIR(file_mode) ? 'S' : '-'), \
			file_mode & 040 ? 'r' : '-', \
			file_mode & 020 ? 'w' : '-', \
			file_mode & 010 ? (S_ISDIR(file_mode) ? 's' : 'x') : (S_ISDIR(file_mode) ? 'S' : '-'), \
			file_mode & 04 ? 'r' : '-', \
			file_mode & 02 ? 'w' : '-', \
			file_mode & 01 ? (S_ISDIR(file_mode) ? 's' : 'x') : (S_ISDIR(file_mode) ? 'S' : '-'), \
			file_size, \
			num_to_month[file_tm.tm_mon%12], \
			file_tm.tm_mday, \
			yt, \
			file_name

	if (!S_ISLNK(file_mode) || link_name[0] == '\0') {
		return snprintf(out, n, LIST_FMT FTPS4_EOL, LIST_ARGS);
	}
	else {
		return snprintf(out, n, LIST_FMT " -> %s" FTPS4_EOL, LIST_ARGS, link_name);
	}

#undef LIST_ARGS
#undef LIST_FMT
}

static void send_LIST(ftps4_client_info_t* client, const char* path)
{
	char buffer[512];
	uint8_t* dentbuf;
	size_t dentbufsize;
	int dfd, dentsize, err, readlinkerr;
	struct dirent* dent, * dend;
	struct stat st;
	time_t cur_time;
	struct tm tm, cur_tm;

	if (stat(path, &st) < 0) {
		client_send_ctrl_msg(client, "550 Invalid directory." FTPS4_EOL);
		return;
	}

	dentbufsize = st.st_blksize;
	log_debug("[FTP Module] dent buffer size = %lx", dentbufsize);

	dfd = open(path, O_RDONLY, 0);
	if (dfd < 0) {
		client_send_ctrl_msg(client, "550 Invalid directory." FTPS4_EOL);
		return;
	}

	dentbuf = (uint8_t*)malloc(dentbufsize);
	memset(dentbuf, 0, dentbufsize);

	client_send_ctrl_msg(client, "150 Opening ASCII mode data transfer for LIST." FTPS4_EOL);

	client_open_data_connection(client);

	time(&cur_time);
	gmtime_s(&cur_time, &cur_tm);

	while ((dentsize = getdents(dfd, (char*)dentbuf, dentbufsize)) > 0) {
		dent = (struct dirent*)dentbuf;
		dend = (struct dirent*)(&dentbuf[dentsize]);

		while (dent != dend) {
			if (dent->d_name[0] != '\0') {
				char full_path[PATH_MAX];
				snprintf(full_path, sizeof(full_path), "%s/%s", path, dent->d_name);

				err = stat(full_path, &st);
				if (err == 0) {
					char link_path[PATH_MAX];
					if (S_ISLNK(st.st_mode)) {
						if ((readlinkerr = readlink(full_path, link_path, sizeof(link_path))) > 0) {
							link_path[readlinkerr] = 0;
						}
						else {
							link_path[0] = 0;
						}
					}

					gmtime_s(&st.st_ctim.tv_sec, &tm);
					gen_list_format(buffer, sizeof(buffer),
						st.st_mode,
						st.st_size,
						tm,
						dent->d_name,
						S_ISLNK(st.st_mode) && link_path[0] != '\0' ? link_path : NULL,
						cur_tm);

					client_send_data_msg(client, buffer);
					memset(buffer, 0, sizeof(buffer));
				}
				else {
					log_debug("[FTP Module] %s stat returned %s", full_path, strerror(errno));
				}
			}
			else {
				log_debug("[FTP Module] got empty dent");
			}
            dent = (struct dirent*)(((char*)dent) + dent->d_reclen);
		}
		memset(dentbuf, 0, dentbufsize);
	}

	close(dfd);
	free(dentbuf);

	log_debug("[FTP Module] Done sending LIST");

	client_close_data_connection(client);
	client_send_ctrl_msg(client, "226 Transfer complete." FTPS4_EOL);
}

static void cmd_LIST_func(ftps4_client_info_t* client)
{
	char list_path[PATH_MAX];
	int list_cur_path = 1;
	int n = !client->recv_cmd_args
		? 0
		: sscanf(client->recv_cmd_args, "%[^\r\n\t]", list_path);

	if (n > 0 && file_exists(list_path))
		list_cur_path = 0;

	if (list_cur_path)
		send_LIST(client, client->cur_path);
	else
		send_LIST(client, list_path);
}

static void cmd_NLST_func(ftps4_client_info_t* client)
{
 cmd_LIST_func(client);
}

static void cmd_PWD_func(ftps4_client_info_t* client)
{
	char msg[PATH_MAX];
	snprintf(msg, sizeof(msg), "257 \"%s\" is the current directory." FTPS4_EOL, client->cur_path);
	client_send_ctrl_msg(client, msg);
}

static void dir_up(char* path)
{
	char* pch;
	size_t len_in = strlen(path);
	if (len_in == 1) {
	root:
		strcpy(path, "/");
		return;
	}
	pch = strrchr(path, '/');
	if (pch == path)
		goto root;
	*pch = '\0';
}

static void cmd_CWD_func(ftps4_client_info_t* client)
{
	char cmd_path[PATH_MAX];
	char tmp_path[PATH_MAX];
	int pd;
	int n = !client->recv_cmd_args
		? 0
		: sscanf(client->recv_cmd_args, "%[^\r\n\t]", cmd_path);

	if (n < 1) {
		client_send_ctrl_msg(client, "500 Syntax error, command unrecognized." FTPS4_EOL);
	}
	else {
		if (strcmp(cmd_path, "/") == 0) {
			strcpy(client->cur_path, cmd_path);
		}
		else  if (strcmp(cmd_path, "..") == 0) {
			dir_up(client->cur_path);
		}
		else {
			if (cmd_path[0] == '/') { /* Full path */
				strcpy(tmp_path, cmd_path);
			}
			else { /* Change dir relative to current dir */
				if (strcmp(client->cur_path, "/") == 0)
					snprintf(tmp_path, sizeof(tmp_path), "%s%s", client->cur_path, cmd_path);
				else
					snprintf(tmp_path, sizeof(tmp_path), "%s/%s", client->cur_path, cmd_path);
			}

			/* If the path is not "/", check if it exists */
			if (strcmp(tmp_path, "/") != 0) {
				/* Check if the path exists */
				pd = open(tmp_path, O_RDONLY, 0);
				if (pd < 0) {
					client_send_ctrl_msg(client, "550 Invalid directory." FTPS4_EOL);
					return;
				}
				close(pd);
			}
			strcpy(client->cur_path, tmp_path);
		}
		client_send_ctrl_msg(client, "250 Requested file action okay, completed." FTPS4_EOL);
	}
}

static void cmd_TYPE_func(ftps4_client_info_t* client)
{
	char data_type;
	char format_control[8];
	int n_args = !client->recv_cmd_args
		? 0
		: sscanf(client->recv_cmd_args, "%c %s", &data_type, format_control);

	if (n_args > 0) {
		switch (data_type) {
		case 'A':
		case 'I':
			client_send_ctrl_msg(client, "200 Okay" FTPS4_EOL);
			break;
		case 'E':
		case 'L':
		default:
			client_send_ctrl_msg(client, "504 Error: bad parameters?" FTPS4_EOL);
			break;
		}
	}
	else {
		client_send_ctrl_msg(client, "504 Error: bad parameters?" FTPS4_EOL);
	}
}

static void cmd_CDUP_func(ftps4_client_info_t* client)
{
	dir_up(client->cur_path);
	client_send_ctrl_msg(client, "200 Command okay." FTPS4_EOL);
}

static void send_file(ftps4_client_info_t* client, const char* path)
{
    int fd;
    unsigned int bytes_read;

    log_debug("[FTP Module] Opening: %s", path);

    if ((fd = open(path, O_RDONLY, 0)) >= 0) {

        lseek(fd, client->restore_point, SEEK_SET);

        std::vector<unsigned char> buffer(file_buf_size); // Allocate memory using std::vector
        if (buffer.empty()) {
            client_send_ctrl_msg(client, "550 Could not allocate memory." FTPS4_EOL);
            return;
        }

        client_open_data_connection(client);
        client_send_ctrl_msg(client, "150 Opening Image mode data transfer." FTPS4_EOL);

        while ((bytes_read = read(fd, buffer.data(), file_buf_size)) > 0) {
            client_send_data_raw(client, buffer.data(), bytes_read);
        }

        close(fd);
        // No need to free the buffer, it will be automatically deallocated
        client->restore_point = 0;
        client_send_ctrl_msg(client, "226 Transfer completed." FTPS4_EOL);
        client_close_data_connection(client);

    }
    else {
        client_send_ctrl_msg(client, "550 File not found." FTPS4_EOL);
    }
}

/* This function generates a FTP full-path valid path with the input path (relative or absolute)
 * from RETR, STOR, DELE, RMD, MKD, RNFR and RNTO commands */
static void gen_ftp_fullpath(ftps4_client_info_t* client, char* path, size_t path_size)
{
	char cmd_path[PATH_MAX];
	int n = !client->recv_cmd_args
		? 0
		: sscanf(client->recv_cmd_args, "%[^\r\n\t]", cmd_path);

	if (n < 1) {
		client_send_ctrl_msg(client, "500 Syntax error, command unrecognized." FTPS4_EOL);
		return;
	}

	if (cmd_path[0] == '/') {
		/* Full path */
		strncpy(path, cmd_path, path_size);
	}
	else {
		/* The file is relative to current dir, so
		 * append the file to the current path */
		snprintf(path, path_size, "%s/%s", client->cur_path, cmd_path);
	}
}

static void cmd_RETR_func(ftps4_client_info_t* client)
{
	char dest_path[PATH_MAX];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
    send_file(client, dest_path);
	
}

static void receive_file(ftps4_client_info_t* client, const char* path)
{
    int fd;
    unsigned int bytes_recv;

    log_debug("[FTP Module] Opening: %s", path);

    int mode = O_CREAT | O_RDWR;
    /* if we resume broken - append missing part
     * else - overwrite file */
    if (client->restore_point) {
        mode = mode | O_APPEND;
    }
    else {
        mode = mode | O_TRUNC;
    }

    if ((fd = open(path, mode, 0777)) >= 0) {

        std::vector<unsigned char> buffer(file_buf_size); // Allocate memory using std::vector
        if (buffer.empty()) {
            client_send_ctrl_msg(client, "550 Could not allocate memory." FTPS4_EOL);
            return;
        }

        client_open_data_connection(client);
        client_send_ctrl_msg(client, "150 Opening Image mode data transfer." FTPS4_EOL);

        while ((bytes_recv = client_recv_data_raw(client, buffer.data(), file_buf_size)) > 0) {
            write(fd, buffer.data(), bytes_recv);
        }

        close(fd);
        // No need to free the buffer, it will be automatically deallocated
        client->restore_point = 0;
        if (bytes_recv == 0) {
            client_send_ctrl_msg(client, "226 Transfer completed." FTPS4_EOL);
        }
        else {
            unlink(path);
            client_send_ctrl_msg(client, "426 Connection closed; transfer aborted." FTPS4_EOL);
        }
        client_close_data_connection(client);

    }
    else {
        client_send_ctrl_msg(client, "550 File not found." FTPS4_EOL);
    }
}

static void cmd_STOR_func(ftps4_client_info_t* client)
{
	char dest_path[PATH_MAX];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	receive_file(client, dest_path);
}

static void delete_file(ftps4_client_info_t* client, const char* path)
{
	log_debug("[FTP Module] Deleting: %s", path);

	if (unlink(path) >= 0) {
		client_send_ctrl_msg(client, "226 File deleted." FTPS4_EOL);
	}
	else {
		client_send_ctrl_msg(client, "550 Could not delete the file." FTPS4_EOL);
	}
}

static void cmd_DELE_func(ftps4_client_info_t* client)
{
	char dest_path[PATH_MAX];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	delete_file(client, dest_path);
}

static void delete_dir(ftps4_client_info_t* client, const char* path)
{
	int ret;
	log_debug("[FTP Module] Deleting: %s", path);
	ret = rmdir(path);
	if (ret >= 0) {
		client_send_ctrl_msg(client, "226 Directory deleted." FTPS4_EOL);
	}
	else if (errno == 66) { /* ENOTEMPTY */
		client_send_ctrl_msg(client, "550 Directory is not empty." FTPS4_EOL);
	}
	else {
		client_send_ctrl_msg(client, "550 Could not delete the directory." FTPS4_EOL);
	}
}

static void cmd_RMD_func(ftps4_client_info_t* client)
{
	char dest_path[PATH_MAX];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	delete_dir(client, dest_path);
}

static void create_dir(ftps4_client_info_t* client, const char* path)
{
	log_debug("[FTP Module] Creating: %s", path);

	if (mkdir(path, 0777) >= 0) {
		client_send_ctrl_msg(client, "226 Directory created." FTPS4_EOL);
	}
	else {
		client_send_ctrl_msg(client, "550 Could not create the directory." FTPS4_EOL);
	}
}

static void cmd_MKD_func(ftps4_client_info_t* client)
{
	char dest_path[PATH_MAX];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	create_dir(client, dest_path);
}

static void cmd_RNFR_func(ftps4_client_info_t* client)
{
	char from_path[PATH_MAX];
	/* Get the origin filename */
	gen_ftp_fullpath(client, from_path, sizeof(from_path));

	/* Check if the file exists */
	if (!file_exists(from_path)) {
		client_send_ctrl_msg(client, "550 The file doesn't exist." FTPS4_EOL);
		return;
	}
	/* The file to be renamed is the received path */
	strcpy(client->rename_path, from_path);
	client_send_ctrl_msg(client, "350 I need the destination name b0ss." FTPS4_EOL);
}

static void cmd_RNTO_func(ftps4_client_info_t* client)
{
	char path_to[PATH_MAX];
	/* Get the destination filename */
	gen_ftp_fullpath(client, path_to, sizeof(path_to));

	log_debug("[FTP Module] Renaming: %s to %s", client->rename_path, path_to);

	if (rename(client->rename_path, path_to) < 0) {
		client_send_ctrl_msg(client, "550 Error renaming the file." FTPS4_EOL);
	}

	client_send_ctrl_msg(client, "226 Rename completed." FTPS4_EOL);
}

static void cmd_SIZ3_func(ftps4_client_info_t* client)
{
	struct stat s;
	char path[PATH_MAX];
	char cmd[64];
	/* Get the filename to retrieve its size */
	gen_ftp_fullpath(client, path, sizeof(path));

	/* Check if the file exists */
	if (stat(path, &s) < 0) {
		client_send_ctrl_msg(client, "550 The file doesn't exist." FTPS4_EOL);
		return;
	}
	/* Send the size of the file */
	sprintf(cmd, "213 %lld" FTPS4_EOL, s.st_size);
	client_send_ctrl_msg(client, cmd);
}

static void cmd_REST_func(ftps4_client_info_t* client)
{
	char cmd[64];
	sscanf(client->recv_buffer, "%*[^ ] %d", &client->restore_point);
	sprintf(cmd, "350 Resuming at %d" FTPS4_EOL, client->restore_point);
	client_send_ctrl_msg(client, cmd);
}

static void cmd_FEAT_func(ftps4_client_info_t* client)
{
	/*So client would know that we support resume */
	client_send_ctrl_msg(client, "211-extensions" FTPS4_EOL);
	client_send_ctrl_msg(client, "REST STREAM" FTPS4_EOL);
	client_send_ctrl_msg(client, "211 end" FTPS4_EOL);
}

static void cmd_APPE_func(ftps4_client_info_t* client)
{
	/* set restore point to not 0
	restore point numeric value only matters if we RETR file from ps4.
	If we STOR or APPE, it is only used to indicate that we want to resume
	a broken transfer */
	client->restore_point = -1;
	char dest_path[PATH_MAX];
	gen_ftp_fullpath(client, dest_path, sizeof(dest_path));
	receive_file(client, dest_path);
}

static cmd_dispatch_func get_dispatch_func(const char* cmd)
{
	int i;
	// Check for commands
	for (i = 0; i < MAX_COMMANDS; i++) {
		if (command_dispatchers[i].valid) {
			if (strcmp(cmd, command_dispatchers[i].cmd) == 0) {
				return command_dispatchers[i].func;
			}
		}
	}
	return NULL;
}

static void client_list_add(ftps4_client_info_t* client)
{
	/* Add the client at the front of the client list */
	scePthreadMutexLock(&client_list_mtx);

	if (client_list == NULL) { /* List is empty */
		client_list = client;
		client->prev = NULL;
		client->next = NULL;
	}
	else {
		client->next = client_list;
		client->next->prev = client;
		client->prev = NULL;
		client_list = client;
	}
	client->restore_point = 0;
	number_clients++;

	scePthreadMutexUnlock(&client_list_mtx);
}

static void client_list_delete(ftps4_client_info_t* client)
{
	/* Remove the client from the client list */
	scePthreadMutexLock(&client_list_mtx);

	if (client->prev) {
		client->prev->next = client->next;
	}
	if (client->next) {
		client->next->prev = client->prev;
	}
	if (client == client_list) {
		client_list = client->next;
	}

	number_clients--;

	scePthreadMutexUnlock(&client_list_mtx);
}

static void client_list_thread_end()
{
	ftps4_client_info_t* it, * next;
	ScePthread client_thid;
	const int data_abort_flags = 0x01 |
		0x02;

	/* Iterate over the client list and close their sockets */
	scePthreadMutexLock(&client_list_mtx);

	it = client_list;

	while (it) {
		next = it->next;
		client_thid = it->thid;

		/* Abort the client's control socket, only abort
		 * receiving data so we can still send control messages */
		sceNetSocketAbort(it->ctrl_sockfd,
			0x01);

		/* If there's an open data connection, abort it */
		if (it->data_con_type != FTP_DATA_CONNECTION_NONE) {
			sceNetSocketAbort(it->data_sockfd, data_abort_flags);
			if (it->data_con_type == FTP_DATA_CONNECTION_PASSIVE) {
				sceNetSocketAbort(it->pasv_sockfd, data_abort_flags);
			}
		}

		/* Wait until the client threads ends */
		scePthreadJoin(client_thid, NULL);

		it = next;
	}

	scePthreadMutexUnlock(&client_list_mtx);
}

static void* client_thread(void* arg)
{
	char cmd[16];
	cmd_dispatch_func dispatch_func;
	ftps4_client_info_t* client = (ftps4_client_info_t*)arg;

	log_debug("[FTP Module] Client thread %i started!", client->num);

	client_send_ctrl_msg(client, "220 FTPS4 Server ready." FTPS4_EOL);

	while (1) {
		memset(client->recv_buffer, 0, sizeof(client->recv_buffer));

		client->n_recv = sceNetRecv(client->ctrl_sockfd, client->recv_buffer, sizeof(client->recv_buffer), 0);
		if (client->n_recv > 0) {

			/* The command is the first chars until the first space */
			sscanf(client->recv_buffer, "%s", cmd);

			client->recv_cmd_args = strchr(client->recv_buffer, ' ');
			if (client->recv_cmd_args)
				client->recv_cmd_args++; /* Skip the space */

			/* Wait 1 ms before sending any data */
			sceKernelUsleep(1 * 1000);

			if ((dispatch_func = get_dispatch_func(cmd))) {
				dispatch_func(client);
			}
			else {
				client_send_ctrl_msg(client, "502 Sorry, command not implemented. :(" FTPS4_EOL);
			}

		}
		else if (client->n_recv == 0) {
			/* Value 0 means connection closed by the remote peer */
			log_info("Connection closed by the client %i.", client->num);
			/* Delete itself from the client list */
			client_list_delete(client);
			break;
		}
		else if (client->n_recv == 0x80410104) {
			/* Socket aborted (ftps4_fini() called) */
			log_info("Client %i socket aborted.", client->num);
			break;
		}
		else {
			/* Other errors */
			log_info("Client %i socket error: 0x%08X", client->num, client->n_recv);
			client_list_delete(client);
			break;
		}
	}

	/* Close the client's socket */
	sceNetSocketClose(client->ctrl_sockfd);

	/* If there's an open data connection, close it */
	if (client->data_con_type != FTP_DATA_CONNECTION_NONE) {
		sceNetSocketClose(client->data_sockfd);
		if (client->data_con_type == FTP_DATA_CONNECTION_PASSIVE) {
			sceNetSocketClose(client->pasv_sockfd);
		}
	}

	log_debug("[FTP Module] Client thread %i exiting!", client->num);

	delete client;

	scePthreadExit(NULL);
	return NULL;
}

static void* server_thread(void* arg)
{
	int ret, enable;
	UNUSED(ret);

	struct sockaddr_in serveraddr;

	log_debug("[FTP Module] Server thread started!");

	/* Create server socket */
	server_sockfd = sceNetSocket("FTPS4_server_sock",
		AF_INET,
		SOCK_STREAM,
		0);

	log_debug("[FTP Module] Server socket fd: %d", server_sockfd);

	enable = 1;
	sceNetSetsockopt(server_sockfd, 0xffff, 0x04, &enable, sizeof(enable));

	/* Fill the server's address */
	serveraddr.sin_len = sizeof(serveraddr);
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = sceNetHtonl(0);
	serveraddr.sin_port = sceNetHtons(ps4_port);

	/* Bind the server's address to the socket */
	ret = sceNetBind(server_sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	log_debug("[FTP Module] sceNetBind(): 0x%08X", ret);

	/* Start listening */
	ret = sceNetListen(server_sockfd, 128);
	log_debug("[FTP Module] sceNetListen(): 0x%08X", ret);

	while (1) {
		/* Accept clients */
		struct sockaddr_in clientaddr;
		int client_sockfd;
		unsigned int addrlen = sizeof(clientaddr);

		log_debug("[FTP Module] Waiting for incoming connections...");

		client_sockfd = sceNetAccept(server_sockfd, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sockfd >= 0) {
			log_debug("[FTP Module] New connection, client fd: 0x%08X", client_sockfd);

			/* Get the client's IP address */
			char remote_ip[16];
			sceNetInetNtop(AF_INET,
				&clientaddr.sin_addr.s_addr,
				remote_ip,
				sizeof(remote_ip));

			log_info("Client %i connected, IP: %s port: %i",
				number_clients, remote_ip, clientaddr.sin_port);

			/* Allocate the ftps4_client_info_t struct for the new client */
			ftps4_client_info_t* client = new ftps4_client_info_t;
			client->num = number_clients;
			client->ctrl_sockfd = client_sockfd;
			client->data_con_type = FTP_DATA_CONNECTION_NONE;
			strcpy(client->cur_path, FTP_DEFAULT_PATH);
			memcpy(&client->addr, &clientaddr, sizeof(client->addr));

			/* Add the new client to the client list */
			client_list_add(client);

			/* Create a new thread for the client */
			char client_thread_name[64];
			sprintf(client_thread_name, "FTPS4_client_%i_thread",
				number_clients);

			/* Create a new thread for the client */
			scePthreadCreate(&client->thid, NULL, client_thread, client, client_thread_name);

			log_debug("[FTP Module] Client %i thread UID: 0x%08X", number_clients, client->thid);

			number_clients++;
		}
		else if (client_sockfd == 0x80410104) {
			log_info("Server socket aborted.");
			break;
		}
		else {
			/* if sceNetAccept returns < 0, it means that the listening
			 * socket has been closed, this means that we want to
			 * finish the server thread */
			log_debug("[FTP Module] Server socket closed, 0x%08X", client_sockfd);
			break;
		}
	}

	log_debug("[FTP Module] Server thread exiting!");

	/* Causing a crash? */
	/*scePthreadExit(NULL);*/
	return NULL;
}

#define add_command(name) ftps4_ext_add_command(#name, cmd_##name##_func)
bool ftps4_init(const char* ip, unsigned short int port)
{
	int i;

	if (ftp_initialized) {
		return false;
	}

	file_buf_size = DEFAULT_FILE_BUF_SIZE;

	/* Save the listening port of the PS4 to a global variable */
	ps4_port = port;

	/* Save the IP of the PS4 to a global variable */
	sceNetInetPton(AF_INET, ip, &ps4_addr);

	/* Create the client list mutex */
	scePthreadMutexInit(&client_list_mtx, NULL, "FTPS4_client_list_mutex");
	log_debug("[FTP Module] Client list mutex UID: 0x%08X", client_list_mtx);

	for (i = 0; i < MAX_COMMANDS; i++) {
		command_dispatchers[i].valid = 0;
	}

	/* Add commands */
	add_command(NOOP);
	add_command(USER);
	add_command(installpkg);
	add_command(PASS);
	add_command(QUIT);
	add_command(SYST);
	add_command(PASV);
	add_command(PORT);
	add_command(LIST);
	add_command(NLST);
	add_command(PWD);
	add_command(CWD);
	add_command(TYPE);
	add_command(CDUP);
	add_command(RETR);
	add_command(STOR);
	add_command(DELE);
	add_command(RMD);
	add_command(MKD);
	add_command(RNFR);
	add_command(RNTO);
	add_command(SIZ3); //size
	add_command(REST);
	add_command(FEAT);
	add_command(APPE);

	/* Create server thread */
	scePthreadCreate(&server_thid, NULL, server_thread, NULL, "FTPS4_server_thread");
	log_debug("[FTP Module] Server thread UID: 0x%08X", server_thid);

	ftp_initialized = 1;

	return true;
}

void ftps4_fini()
{
	if (ftp_initialized) {
		/* Necessary to get sceNetAccept to notice the close on PS4? */
		sceNetSocketAbort(server_sockfd, 0);
		/* In order to "stop" the blocking sceNetAccept,
		 * we have to close the server socket; this way
		 * the accept call will return an error */
		sceNetSocketClose(server_sockfd);

		/* Wait until the server threads ends */
		scePthreadJoin(server_thid, NULL);

		/* To close the clients we have to do the same:
		 * we have to iterate over all the clients
		 * and shutdown their sockets */
		client_list_thread_end();

		/* Delete the client list mutex */
		scePthreadMutexDestroy(&client_list_mtx);

		client_list = NULL;
		number_clients = 0;

		ftp_initialized = 0;
	}
}

int ftps4_is_initialized()
{
	return ftp_initialized;
}

void ftps4_set_file_buf_size(unsigned int size)
{
	file_buf_size = size;
}

int ftps4_ext_add_command(const char* cmd, cmd_dispatch_func func)
{
	int i;
	for (i = 0; i < MAX_COMMANDS; i++) {
		if (!command_dispatchers[i].valid) {
			command_dispatchers[i].cmd = cmd;
			command_dispatchers[i].func = func;
			command_dispatchers[i].valid = 1;
			return 1;
		}
	}
	return 0;
}

int ftps4_ext_del_command(const char* cmd)
{
	int i;
	for (i = 0; i < MAX_COMMANDS; i++) {
		if (strcmp(cmd, command_dispatchers[i].cmd) == 0) {
			command_dispatchers[i].valid = 0;
			return 1;
		}
	}
	return 0;
}

void ftps4_ext_client_send_ctrl_msg(ftps4_client_info_t* client, const char* msg)
{
	client_send_ctrl_msg(client, msg);
}

void ftps4_ext_client_send_data_msg(ftps4_client_info_t* client, const char* str)
{
	client_send_data_msg(client, str);
}

void ftps4_gen_ftp_fullpath(ftps4_client_info_t* client, char* path, size_t path_size)
{
	gen_ftp_fullpath(client, path, path_size);
}

int get_ip_address(char* ip_address)
{
	int ret;
	SceNetCtlInfo info;

	ret = sceNetCtlInit();
	if (ret < 0)
		goto error;

	ret = sceNetCtlGetInfo(14, &info);
	if (ret < 0)
		goto error;

	memcpy(ip_address, info.ip_address, sizeof(info.ip_address));

	sceNetCtlTerm();

	return ret;

error:
	ip_address = NULL;
	return -1;
}
bool ftp_started = false;

bool StartFTP() {
	ftp_started = false;
	if (get_ip_address(&ip_address[0]) < 0)
		return false;

   return (ftp_started = ftps4_init(&ip_address[0], 21));
}
void ShutdownFTP() {
	ftps4_fini();
}
void check_ftp_addr_change(){

	char func_ip_address[16];
	if (get_ip_address(&func_ip_address[0]) < 0 || !ftp_started)
		return;

	if (strcmp(&ip_address[0], &func_ip_address[0]) != 0) {
		log_debug("[FTP Module] IP Address changed, restarting FTP server");
		ShutdownFTP();
		StartFTP();
	}
}
