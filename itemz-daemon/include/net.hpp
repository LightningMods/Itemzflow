#ifndef _SCE_SSL_H_
#define _SCE_SSL_H_

#pragma once
#include <stdint.h>
#include "defines.h"
#include <orbis/libSceNet.h>


#ifdef __cplusplus 
extern "C" {
#endif

#define ORBIS_NET_CTL_INFO_DEVICE           1
#define ORBIS_NET_CTL_INFO_ETHER_ADDR       2
#define ORBIS_NET_CTL_INFO_MTU          3
#define ORBIS_NET_CTL_INFO_LINK         4
#define ORBIS_NET_CTL_INFO_BSSID            5
#define ORBIS_NET_CTL_INFO_SSID         6
#define ORBIS_NET_CTL_INFO_WIFI_SECURITY        7
#define ORBIS_NET_CTL_INFO_RSSI_DBM     8
#define ORBIS_NET_CTL_INFO_RSSI_PERCENTAGE  9
#define ORBIS_NET_CTL_INFO_CHANNEL      10
#define ORBIS_NET_CTL_INFO_IP_CONFIG        11
#define ORBIS_NET_CTL_INFO_DHCP_HOSTNAME        12
#define ORBIS_NET_CTL_INFO_PPPOE_AUTH_NAME  13
#define ORBIS_NET_CTL_INFO_IP_ADDRESS       14
#define ORBIS_NET_CTL_INFO_NETMASK      15
#define ORBIS_NET_CTL_INFO_DEFAULT_ROUTE        16
#define ORBIS_NET_CTL_INFO_PRIMARY_DNS      17
#define ORBIS_NET_CTL_INFO_SECONDARY_DNS        18
#define ORBIS_NET_CTL_INFO_HTTP_PROXY_CONFIG    19
#define ORBIS_NET_CTL_INFO_HTTP_PROXY_SERVER    20
#define ORBIS_NET_CTL_INFO_HTTP_PROXY_PORT  21
#define ORBIS_NET_CTL_INFO_RESERVED1        22
#define ORBIS_NET_CTL_INFO_RESERVED2        23

typedef union OrbisNetCtlInfo {
    uint32_t device;
    char ether_addr[6];
    uint32_t mtu;
    uint32_t link;
    char bssid[6];
    char ssid[33];
    uint32_t wifi_security;
    uint8_t rssi_dbm;
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
} OrbisNetCtlInfo;

int sceNetCtlGetInfo(int, void *);
int sceNetCtlInit();

typedef void* OrbisHttpEpollHandle;

#define	ORBIS_HTTP_NB_EVENT_IN			0x00000001
#define	ORBIS_HTTP_NB_EVENT_OUT			0x00000002
#define	ORBIS_HTTP_NB_EVENT_SOCK_ERR		0x00000008
#define	ORBIS_HTTP_NB_EVENT_HUP			0x00000010
#define ORBIS_HTTP_NB_EVENT_ICM			0x00000020
#define	ORBIS_HTTP_NB_EVENT_RESOLVED		0x00010000
#define	ORBIS_HTTP_NB_EVENT_RESOLVER_ERR	0x00020000

typedef struct OrbisHttpNBEvent {
	uint32_t	events;
	uint32_t	eventDetail;
	int			id;
	void*		userArg;
} OrbisHttpNBEvent;


#define ORBIS_HTTP_DEFAULT_RESOLVER_TIMEOUT		(0)	// libnet default
#define ORBIS_HTTP_DEFAULT_RESOLVER_RETRY			(0)	// libnet default
#define ORBIS_HTTP_DEFAULT_CONNECT_TIMEOUT		(30* 1000 * 1000U)
#define ORBIS_HTTP_DEFAULT_SEND_TIMEOUT			(120* 1000 * 1000U)
#define ORBIS_HTTP_DEFAULT_RECV_TIMEOUT			(120* 1000 * 1000U)
#define ORBIS_HTTP_DEFAULT_RECV_BLOCK_SIZE		(1500U)
#define ORBIS_HTTP_DEFAULT_RESPONSE_HEADER_MAX	(5000U)
#define ORBIS_HTTP_DEFAULT_REDIRECT_MAX			(6U)
#define ORBIS_HTTP_DEFAULT_TRY_AUTH_MAX			(6U)

#define ORBIS_HTTP_TRUE				(int)(1)
#define ORBIS_HTTP_FALSE				(int)(0)

#define ORBIS_HTTP_ENABLE				ORBIS_HTTP_TRUE
#define ORBIS_HTTP_DISABLE			ORBIS_HTTP_FALSE

#define ORBIS_HTTP_USERNAME_MAX_SIZE	256
#define ORBIS_HTTP_PASSWORD_MAX_SIZE	256


#define ORBIS_HTTP_TRUE				(int)(1)
#define ORBIS_HTTP_FALSE				(int)(0)

#define ORBIS_HTTP_ENABLE				ORBIS_HTTP_TRUE
#define ORBIS_HTTP_DISABLE			ORBIS_HTTP_FALSE

#define ORBIS_HTTP_USERNAME_MAX_SIZE	256
#define ORBIS_HTTP_PASSWORD_MAX_SIZE	256


typedef enum{
	ORBIS_HTTP_VERSION_1_0=1,
	ORBIS_HTTP_VERSION_1_1
} OrbisHttpHttpVersion;

typedef enum{
	ORBIS_HTTP_PROXY_AUTO,
	ORBIS_HTTP_PROXY_MANUAL
} OrbisHttpProxyMode;

typedef enum{
	ORBIS_HTTP_HEADER_OVERWRITE,
	ORBIS_HTTP_HEADER_ADD
} OrbisHttpAddHeaderMode;

typedef enum{
	ORBIS_HTTP_AUTH_BASIC,
	ORBIS_HTTP_AUTH_DIGEST,
	ORBIS_HTTP_AUTH_RESERVED0,
	ORBIS_HTTP_AUTH_RESERVED1,
	ORBIS_HTTP_AUTH_RESERVED2
} OrbisHttpAuthType;

typedef enum {
	ORBIS_HTTP_CONTENTLEN_EXIST,
	ORBIS_HTTP_CONTENTLEN_NOT_FOUND,
	ORBIS_HTTP_CONTENTLEN_CHUNK_ENC
} OrbisHttpContentLengthType;

typedef enum {
	ORBIS_HTTP_REQUEST_STATUS_CONNECTION_RESERVED,
	ORBIS_HTTP_REQUEST_STATUS_RESOLVING_NAME,
	ORBIS_HTTP_REQUEST_STATUS_NAME_RESOLVED,
	ORBIS_HTTP_REQUEST_STATUS_CONNECTING,
	ORBIS_HTTP_REQUEST_STATUS_CONNECTED,
	ORBIS_HTTP_REQUEST_STATUS_TLS_CONNECTING,
	ORBIS_HTTP_REQUEST_STATUS_TLS_CONNECTED,
	ORBIS_HTTP_REQUEST_STATUS_SENDING_REQUEST,
	ORBIS_HTTP_REQUEST_STATUS_REQUEST_SENT,
	ORBIS_HTTP_REQUEST_STATUS_RECEIVING_HEADER,
	ORBIS_HTTP_REQUEST_STATUS_HEADER_RECEIVED,
} OrbisHttpRequestStatus;


#define ORBIS_HTTP_INVALID_ID	0


typedef int (*OrbisHttpAuthInfoCallback)(
	int request,
	OrbisHttpAuthType authType,
	const char *realm,
	char *username,
	char *password,
	int  isNeedEntity,
	uint8_t **entityBody,
	size_t *entitySize,
	int *isSave,
	void *userArg);

typedef int (*OrbisHttpRedirectCallback)(
	int request,
	int32_t statusCode,
	int32_t *method,
	const char *location,
	void *userArg);

typedef void (*OrbisHttpRequestStatusCallback)(
	int request,
	OrbisHttpRequestStatus requestStatus,
	void *userArg);

typedef struct OrbisHttpMemoryPoolStats{
	size_t		poolSize;
	size_t		maxInuseSize;
	size_t		currentInuseSize;
	int32_t	reserved;
} OrbisHttpMemoryPoolStats;



// Empty Comment
int sceHttpAbortRequest(int reqId);
// Empty Comment
void sceHttpAbortRequestForce();
// Empty Comment
int sceHttpAbortWaitRequest(OrbisHttpEpollHandle eh);
// Empty Comment
void sceHttpAddCookie();
// Empty Comment
int sceHttpAddRequestHeader(int id, const char *name, const char *value, uint32_t mode);
// Empty Comment
void sceHttpAddRequestHeaderRaw();
// Empty Comment
void sceHttpAuthCacheExport();
// Empty Comment
int sceHttpAuthCacheFlush(int libhttpCtxId);
// Empty Comment
void sceHttpAuthCacheImport();
// Empty Comment
void sceHttpCookieExport();
// Empty Comment
void sceHttpCookieFlush();
// Empty Comment
void sceHttpCookieImport();
// Empty Comment
int sceHttpCreateConnection(int tmplId, const char *serverName, const char *scheme, uint16_t port, int isEnableKeepalive);
// Empty Comment
int sceHttpCreateConnectionWithURL(int tmplId, const char *url, int isEnableKeepalive);
// Empty Comment
int sceHttpCreateEpoll(int libhttpCtxId, OrbisHttpEpollHandle* eh);
// Empty Comment
int sceHttpCreateRequest(int connId, int method, const char *path, uint64_t	contentLength);
// Empty Comment
int sceHttpCreateRequest2(int connId, const char* method, const char *path, uint64_t contentLength);
// Empty Comment
int sceHttpCreateRequestWithURL(int connId, int method, const char *url, uint64_t contentLength);
// Empty Comment
int sceHttpCreateRequestWithURL2(int connId, const char* method, const char *url, uint64_t contentLength);
// Empty Comment
int sceHttpCreateTemplate(int libhttpCtxId, const char *userAgent, int httpVer, int isAutoProxyConf);
// Empty Comment
void sceHttpDbgGetConnectionStat();
// Empty Comment
void sceHttpDbgGetRequestStat();
// Empty Comment
void sceHttpDbgSetPrintf();
// Empty Comment
void sceHttpDbgShowConnectionStat();
// Empty Comment
void sceHttpDbgShowMemoryPoolStat();
// Empty Comment
void sceHttpDbgShowRequestStat();
// Empty Comment
void sceHttpDbgShowStat();
// Empty Comment
int sceHttpDeleteConnection(int connId);
// Empty Comment
int sceHttpDeleteRequest(int reqId);
// Empty Comment
int sceHttpDeleteTemplate(int tmplId);
// Empty Comment
int sceHttpDestroyEpoll(int libhttpCtxId, OrbisHttpEpollHandle eh);
// Empty Comment
void sceHttpGetAcceptEncodingGZIPEnabled();
// Empty Comment
int sceHttpGetAllResponseHeaders(int reqId, char **header, size_t *headerSize);
// Empty Comment
int sceHttpGetAuthEnabled(int id, int *isEnable);
// Empty Comment
int sceHttpGetAutoRedirect(int id, int *isEnable);
// Empty Comment
void sceHttpGetCookie();
// Empty Comment
void sceHttpGetCookieEnabled();
// Empty Comment
void sceHttpGetCookieStats();
// Empty Comment
int sceHttpGetEpoll(int id, OrbisHttpEpollHandle* eh, void **userArg);
// Empty Comment
void sceHttpGetEpollId();
// Empty Comment
int sceHttpGetLastErrno(int reqId, int* errNum);
// Empty Comment
int sceHttpGetMemoryPoolStats(int libhttpCtxId, OrbisHttpMemoryPoolStats* currentStat);
// Empty Comment
int sceHttpGetNonblock(int id, int *isEnable);
// Empty Comment
int sceHttpGetResponseContentLength(int reqId, int* result, uint64_t *contentLength);
// Empty Comment
int sceHttpGetStatusCode(int reqId, int *statusCode);
// Empty Comment
int sceHttpInit(int libnetMemId, int libsslCtxId, size_t poolSize);
// Empty Comment
int sceHttpParseResponseHeader(const char *header, size_t headerLen, const char *fieldStr, const char **fieldValue, size_t *valueLen);
// Empty Comment
int sceHttpParseStatusLine(const char *statusLine, size_t lineLen, int *httpMajorVer, int *httpMinorVer, int *responseCode, const char **reasonPhras);
// Empty Comment
int sceHttpReadData(int reqId, void *data, size_t size);
// Empty Comment
int sceHttpRedirectCacheFlush(int libhttpCtxId);
// Empty Comment
int sceHttpRemoveRequestHeader(int id, const char *name);
// Empty Comment
void sceHttpRequestGetAllHeaders();
// Empty Comment
void sceHttpsDisableOption();
// Empty Comment
void sceHttpsDisableOptionPrivate();
// Empty Comment
void sceHttpsEnableOption();
// Empty Comment
void sceHttpsEnableOptionPrivate();
// Empty Comment
int sceHttpSendRequest(int reqId, const void *postData, size_t size);
// Empty Comment
void sceHttpSetAcceptEncodingGZIPEnabled();
// Empty Comment
int sceHttpSetAuthEnabled(int id, int isEnable);
// Empty Comment
int sceHttpSetAuthInfoCallback(int id, OrbisHttpAuthInfoCallback cbfunc, void *userArg);
// Empty Comment
int sceHttpSetAutoRedirect(int id, int isEnable);
// Empty Comment
int sceHttpSetChunkedTransferEnabled(int id, int isEnable);
// Empty Comment
int sceHttpSetConnectTimeOut(int id, uint32_t usec);
// Empty Comment
void sceHttpSetCookieEnabled();
// Empty Comment
void sceHttpSetCookieMaxNum();
// Empty Comment
void sceHttpSetCookieMaxNumPerDomain();
// Empty Comment
void sceHttpSetCookieMaxSize();
// Empty Comment
void sceHttpSetCookieRecvCallback();
// Empty Comment
void sceHttpSetCookieSendCallback();
// Empty Comment
void sceHttpSetCookieTotalMaxSize();
// Empty Comment
void sceHttpSetDefaultAcceptEncodingGZIPEnabled();
// Empty Comment
void sceHttpSetEpoll();
// Empty Comment
void sceHttpSetEpollId();
// Empty Comment
int sceHttpSetInflateGZIPEnabled(int id, int isEnable);
// Empty Comment
int sceHttpSetNonblock(int id, int isEnable);
// Empty Comment
void sceHttpSetPolicyOption();
// Empty Comment
void sceHttpSetPriorityOption();
// Empty Comment
void sceHttpSetProxy();
// Empty Comment
void sceHttpSetRecvBlockSize();
// Empty Comment
int sceHttpSetRecvTimeOut(int id, uint32_t usec);
// Empty Comment
int sceHttpSetRedirectCallback(int id, OrbisHttpRedirectCallback cbfunc, void *userArg);
// Empty Comment
int sceHttpSetRequestContentLength(int id, uint64_t contentLength);
// Empty Comment
int sceHttpSetResolveRetry(int id, int retry);
// Empty Comment
int sceHttpSetResolveTimeOut(int id, uint32_t usec);
// Empty Comment
int sceHttpSetResponseHeaderMaxSize(int id, size_t headerSize);
// Empty Comment
int sceHttpSetSendTimeOut(int id, uint32_t usec);
// Empty Comment
void sceHttpsFreeCaList();
// Empty Comment
void sceHttpsGetCaList();
// Empty Comment
void sceHttpsGetSslError();
// Empty Comment
void sceHttpsLoadCert();
typedef int (*HttpsCallback)(int libsslCtxId, unsigned int verifyErr, void* const sslCert[], int certNum, void* userArg);
int sceHttpsSetSslCallback(int id, HttpsCallback cbfunc, void* userArg);
// Empty Comment
void sceHttpsSetSslVersion();
// Empty Comment
void sceHttpsUnloadCert();
// Empty Comment
int sceHttpTerm(int libhttpCtxId);
// Empty Comment
int sceHttpTryGetNonblock(int id, int *isEnable);
// Empty Comment
int sceHttpTrySetNonblock(int id, int isEnable);
// Empty Comment
int sceHttpUnsetEpoll(int id);
// Empty Comment
void sceHttpUriBuild();
// Empty Comment
void sceHttpUriCopy();
// Empty Comment
void sceHttpUriEscape();
// Empty Comment
void sceHttpUriMerge();
// Empty Comment
void sceHttpUriParse();
// Empty Comment
void sceHttpUriSweepPath();
// Empty Comment
void sceHttpUriUnescape();
// Empty Comment
int sceHttpWaitRequest(OrbisHttpEpollHandle eh, OrbisHttpNBEvent* nbev, int maxevents, int timeout);


void sceNetCtlTerm();

int sceNetPoolCreate(
	const char *name,
	int size,
	int flags
);
int sceNetInit();

#endif


#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
#include <string>
#include <vector>
struct clientArgs {
    std::string ip;
    int socket;
    int cl_nmb;

};
#define DAEMON_BUFF_MAX 0x100

struct IPCMessage{
    int magic = 0xDEADBABE; 
    cmd cmd;
    IPC_Errors error = NO_ERROR;
    char msg[DAEMON_BUFF_MAX] = {0};
};
void* ipc_client(void* args);
extern std::string fuse_session_ip;
struct sockaddr_in networkAdress(uint16_t port);
int networkListen(const char* soc_path);
int networkAccept(int socket);
int networkReceiveData(int socket, void* buffer, int32_t size);
int networkSendData(int socket, void* buffer, int32_t size);
int networkSendDebugData(uint8_t* buffer, int32_t size);
int networkCloseConnection(int socket);
int networkCloseDebugConnection();

#endif