#include <stdarg.h>
#include <stdlib.h> // calloc
#include <atomic>
#include "jsmn.h"
#include "defines.h"
#include "net.h"
#define ORBIS_TRUE 1

struct dl_args {
    const char *src,
               *dst;
    int req,
        idx,    // thread index!
        connid,
        tmpid,
        status, // thread status
        g_idx,
        ret;  // global item index in icon panel
    double      progress;
    uint64_t    contentLength; 
    void *unused; // for cross compat with _v2
    bool is_threaded;
} dl_args;

std::atomic_ulong g_progress = 0;

int libnetMemId  = 0xFF,
    libsslCtxId  = 0xFF,
    libhttpCtxId = 0xFF,
    statusCode   = 0xFF;

int  contentLengthType;
uint64_t contentLength;
static bool curl_initialized = false;
CURL *(*curl_easy_init)() = NULL;
CURLcode (*curl_easy_setopt)(CURL *curl, CURLoption option, ...) = NULL;
CURLcode (*curl_easy_perform)(CURL *curl) = NULL;
void (*curl_easy_cleanup)(CURL *curl) = NULL;
CURLcode (*curl_easy_getinfo)(CURL *handle, CURLINFO info, long *codep) = NULL;
const char *(*curl_easy_strerror)(CURLcode errornum) = NULL;
CURLcode (*curl_global_init)(long flags) = NULL;
char *(*curl_version)() = NULL;



void DL_ERROR(const char* name, int statusCode, struct dl_args *i)
{
    if (!i->is_threaded) 
        log_warn( "[HTTP] Download Failed with code HEX: %x Int: %i from Function %s src: %s", statusCode, statusCode, name, i->src);
    else 
        msgok(WARNING, fmt::format("{0:}\n\nHEX: {1:#x} Int: {2:d}\nfrom Function {3:} src: {4:}", getLangSTR(DL_FAILED_W), statusCode, statusCode, name, i->src));
}


bool init_curl(){

    if((curl_global_init = (CURLcode (*)(long))prx_func_loader(asset_path("../Media/curl.prx"), "curl_global_init")) == NULL){
       return false;
    }
    if((curl_version = (char *(*)())prx_func_loader(asset_path("../Media/curl.prx"), "curl_version")) == NULL){
       return false;
    }
    if((curl_easy_init = (CURL *(*)())prx_func_loader(asset_path("../Media/curl.prx"), "curl_easy_init")) == NULL){
       return false;
    }
    if((curl_easy_setopt = (CURLcode (*)(CURL *curl, CURLoption option, ...))prx_func_loader(asset_path("../Media/curl.prx"), "curl_easy_setopt")) == NULL){
       return false;
    }
    if((curl_easy_perform = (CURLcode (*)(CURL *curl))prx_func_loader(asset_path("../Media/curl.prx"), "curl_easy_perform")) == NULL){
       return false;
    }
    if((curl_easy_cleanup = (void (*)(CURL *curl))prx_func_loader(asset_path("../Media/curl.prx"), "curl_easy_cleanup")) == NULL){
       return false;
    }
    if((curl_easy_getinfo = (CURLcode (*)(CURL *handle, CURLINFO info, long *codep))prx_func_loader(asset_path("../Media/curl.prx"), "curl_easy_getinfo")) == NULL){
       return false;
    }
    if((curl_easy_strerror = (const char *(*)(CURLcode errornum))prx_func_loader(asset_path("../Media/curl.prx"), "curl_easy_strerror")) == NULL){
       return false;
    }

    int ress = curl_global_init(CURL_GLOBAL_SSL);
    if(ress < 0){
        log_info("curl_global_init failed");
        return false;
    }
    else{
        log_info("curl_version | %s", curl_version());
        curl_initialized = true;
        return true;
    }

    return false;
}
static int update_progress(void *p, int64_t dltotal, int64_t dlnow, int64_t ultotal, int64_t ulnow)
{
    //only show progress every 10mbs to avoid spamming the log
    if (dltotal > 0 && dlnow > 0 && dlnow % MB(5) == 0)
        log_info("[HTTP] Download Progress: %i/%i", dlnow, dltotal);

    return 0;
}

long curlDownloadFile(const char* url, const char* file)
{
	CURL *curl = NULL;

    if(!curl_initialized){
       log_error("curl not initialized");
       return ITEMZCORE_GENERAL_ERROR;
    }

	FILE* imageFD = fopen(file, "wb");
	if(!imageFD){
        log_error("Failed to open file: %s | %s", file, strerror(errno));
		return ITEMZCORE_GENERAL_ERROR;
	}

	CURLcode res  = (CURLcode)-1;
    log_info("Downloading: %s | %p | %p", url, curl_easy_init, curl_easy_strerror);
	curl = curl_easy_init();
    log_info("curl_easy_init: %p", curl);
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		// Set user agent string
        std::string ua = fmt::format("Itemzflow GM/VER: {0:d}.{1:d} FW: {2:#x}", VERSION_MAJOR, VERSION_MINOR, ps4_fw_version());
        log_debug("[HTTP] User Agent set to %s", ua.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
		// not sure how to use this when enabled
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		// not sure how to use this when enabled
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		// Set SSL VERSION to TLS 1.2
		curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
		// Set timeout for the connection to build
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
		// Follow redirects (?)
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		// The function that will be used to write the data 
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
		// The data filedescriptor which will be written to
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, imageFD);
        // maximum number of redirects allowed
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 20);
        // request using SSL for the FTP transfer if available
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);

        /* pass the struct pointer into the xferinfo function */
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, update_progress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

		// Perform the request
		res = curl_easy_perform(curl);
		if(res != CURLE_OK){
			log_info("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		}else{
    		long httpresponsecode = 0;
    		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpresponsecode);
            // close filedescriptor
	        fclose(imageFD), imageFD = NULL;
            // cleanup
	        curl_easy_cleanup(curl);
            log_info("httpresponsecode: %lu", httpresponsecode);
			return httpresponsecode;
		}

	}else{
		log_error("curl_easy_init() failed: %s", curl_easy_strerror(res));
	}

	// close filedescriptor
	fclose(imageFD);
	// cleanup
	curl_easy_cleanup(curl);
    unlink(file);

    return res;
}

// the _v2 version is used in download panel
int dl_from_url(const char *url_, const char *dst_)
{
    long code = curlDownloadFile(url_, dst_);
    if ((int)code != 200){
        log_info("curlDownloadFile failed: %d", code);
        return (int)code;
    }
    else
        log_info("curlDownloadFile success");

    return 0;
}
#define BUFFER_SIZE (0x10000) /* 256kB */

struct write_result {
char *data;
int pos;
};

static size_t curl_write( void *ptr, size_t size, size_t nmemb, void *stream) {

struct write_result *result = (struct write_result *)stream;

/* Will we overflow on this write? */
if(result->pos + size * nmemb >= BUFFER_SIZE - 1) {
   log_error("curl error: too small buffer\n");
   return 0;
}

/* Copy curl's stream buffer into our own buffer */
memcpy(result->data + result->pos, ptr, size * nmemb);

/* Advance the position */
result->pos += size * nmemb;

return size * nmemb;

}

std::string check_from_url(std::string &url_)
{
    char  RES_STR[255];
    char  JSON[BUFFER_SIZE];
    jsmn_parser p;
    int r = -1;
    jsmntok_t t[128]; /* We expect no more than 128 tokens */
    CURL* curl;
    CURLcode res;
    long http_code = 0;
    
    memset(JSON, 0, sizeof(JSON));

   if(!curl_initialized){
       log_error("curl not initialized");
       return std::string();
    }
   
    std::string ua = fmt::format("Itemzflow GM/VER: {0:d}.{1:d} FW: {2:#x}", VERSION_MAJOR, VERSION_MINOR, ps4_fw_version());
    curl = curl_easy_init();
    if (curl)
    {
        
       struct write_result write_result = {
        .data = &JSON[0],
        .pos = 0
       };
       curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
       curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
       curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
       curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
       curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
       curl_easy_setopt(curl, CURLOPT_NOPROGRESS,   1l);
       curl_easy_setopt(curl, CURLOPT_NOBODY, 0l);
       curl_easy_setopt(curl, CURLOPT_HEADER, 0L); 
       curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
       curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

       res = curl_easy_perform(curl);
       if(!res) {
          log_info("RESPONSE: %s", JSON);
       }
       else{
        log_warn( "[StoreCore][HTTP] curl_easy_perform() failed with code %i from Function %s url: %s : error: %s", res, "ini_dl_req", url_.c_str(), curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return std::string();
       }
    
       curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
       if(http_code != 200){
          curl_easy_cleanup(curl);
          return std::string();
       }
       curl_easy_cleanup(curl);
    }
    else{
        log_warn( "[StoreCore][HTTP] curl_easy_perform() failed Function %s url: %s", "ini_dl_req", url_.c_str());
        curl_easy_cleanup(curl);
        return std::string();
    }

    jsmn_init(&p);
    r = jsmn_parse(&p, JSON, strlen(JSON), t, sizeof(t) / sizeof(t[0]));
    if (r  < 0) {
        log_error("could not parse Update json");
        return std::string();
    }

    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT) {
        log_error("JSON Update Object not found");
        return std::string();
    }

    for (int i = 1; i < r; i++) {
        if (jsoneq(JSON, &t[i], "hash") == 0) {
            snprintf(RES_STR, 255, "%.*s", t[i + 1].end - t[i + 1].start, JSON + t[i + 1].start);
            return std::string(RES_STR);
        }
        else
            log_error("Update format not found");
    }//
    return std::string();
}
