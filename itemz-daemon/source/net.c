#include "defines.h"
#include <utils.h>
#include <sys/signal.h>
#include "log.h"
#include <ps4sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define INVAIL -1
#define IPC_SOC "/system_tmp/IPC_Socket"
int DaemonSocket = 0;

struct sockaddr_in networkAdress(uint16_t port)
{
	struct sockaddr_in address;
	address.sin_len = sizeof(address);
	address.sin_family = AF_INET;
	address.sin_port = sceNetHtons(port);
	memset(address.sin_zero, 0, sizeof(address.sin_zero));
	return address;
}

int networkListen(const char* soc_path)
{
	struct sockaddr_un server;
	unlink(soc_path);
	log_info("[Daemon] Deleted Socket...");
	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
	{
		log_info("[Daemon] Socket failed! %s", strerror(errno));
		return INVAIL;
	}

	memset(&server, 0, sizeof(server));
	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, soc_path);

	int r = bind(s, (struct sockaddr*)&server, SUN_LEN(&server));
	if (r < 0)
	{
		log_info("[Daemon] Bind failed! %s", strerror(errno));
		return INVAIL;
	}

	log_info("Socket has name %s", server.sun_path);

	r = listen(s, 100);
	if (r < 0)
	{
		log_info("[Daemon] listen failed! %s", strerror(errno));
		return INVAIL;
	}

	return s;
}

int networkAccept(int socket)
{
	touch_file("/system_tmp/IPC_init");
	return accept(socket, 0, 0);
}


int networkReceiveData(int socket, uint8_t* buffer, int32_t size)
{
	return recv(socket, buffer, size, 0);
}

int networkSendData(int socket, uint8_t* buffer, int32_t size)
{
	return send(socket, buffer, size, MSG_NOSIGNAL);
}

int networkSendDebugData(uint8_t* buffer, int32_t size)
{
	return networkSendData(DaemonSocket, buffer, size);
}

int networkCloseConnection(int socket)
{
	return close(socket);
}

int networkCloseDebugConnection()
{
	return networkCloseConnection(DaemonSocket);
}

bool touch_file(char* destfile)
{
	int fd = open(destfile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	if (fd > 0) {
		close(fd);
		return true;
	}
	else
		return false;
}

void *IPC_loop(void* args)
{
	// Listen on port
	int serverSocket = networkListen(IPC_SOC);
	if (serverSocket < 0)
	{
		log_debug("[Daemon IPC] networkListen error %s", strerror(errno));
		return NULL;
	}


	// Keep accepting client connections
	int cli_new = 0;
	while (true)
	{
		// Accept a client connection
		int clientSocket = networkAccept(serverSocket);
		if (clientSocket < 0)
		{
			log_debug("[Daemon IPC] networkAccept error %s", strerror(errno));
			goto clean;
		}

		log_debug("[Daemon IPC] Connection Accepted");


		log_info("[Daemon IPC] cl_nmb %i", cli_new);
		// Build data to send to thread
		struct clientArgs* clientParams = (struct clientArgs*)malloc(sizeof(struct clientArgs));
		clientParams->ip = "localhost";
		clientParams->socket = clientSocket;
		clientParams->cl_nmb = cli_new;

		log_info("[Daemon IPC] clientParams->cl_nmb %i", clientParams->cl_nmb);

		// Handle client on a thread
		ScePthread thread;
		scePthreadCreate(&thread, NULL, ipc_client, (void*)clientParams, "IPC_SERVER_THREAD");
		cli_new++;
	}

	clean:
	// Close Server Socket
	networkCloseConnection(serverSocket);
	scePthreadExit(NULL);
	return NULL;
}
