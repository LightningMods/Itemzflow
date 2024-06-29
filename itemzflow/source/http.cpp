#include "defines.h"
#include "json.hpp"
#include "net.h"
#include "patcher.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdarg.h>
#include <stdlib.h> // calloc

#define ORBIS_TRUE 1

struct dl_args {
  const char *src, *dst;
  int req,
      idx, // thread index!
      connid, tmpid,
      status, // thread status
      g_idx,
      ret; // global item index in icon panel
  double progress;
  uint64_t contentLength;
  void *unused; // for cross compat with _v2
  bool is_threaded;
} dl_args;

std::atomic_ulong g_progress = 0;

int libnetMemId = 0xFF, libsslCtxId = 0xFF, libhttpCtxId = 0xFF,
    statusCode = 0xFF;

int contentLengthType;
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
void (*curl_global_cleanup)(void) = NULL;


/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/


/* returns last node in linked list */
static struct curl_slist *slist_get_last(struct curl_slist *list)
{
  struct curl_slist     *item;

  /* if caller passed us a NULL, return now */
  if(!list)
    return NULL;

  /* loop through to find the last item */
  item = list;
  while(item->next) {
    item = item->next;
  }
  return item;
}

/*
 * Curl_slist_append_nodup() appends a string to the linked list. Rather than
 * copying the string in dynamic storage, it takes its ownership. The string
 * should have been malloc()ated. Curl_slist_append_nodup always returns
 * the address of the first record, so that you can use this function as an
 * initialization function as well as an append function.
 * If an error occurs, NULL is returned and the string argument is NOT
 * released.
 */
struct curl_slist *Curl_slist_append_nodup(struct curl_slist *list, char *data)
{
  struct curl_slist     *last;
  struct curl_slist     *new_item;

 // DEBUGASSERT(data);

  new_item = (struct curl_slist*)malloc(sizeof(struct curl_slist));
  if(!new_item)
    return NULL;

  new_item->next = NULL;
  new_item->data = data;

  /* if this is the first item, then new_item *is* the list */
  if(!list)
    return new_item;

  last = slist_get_last(list);
  last->next = new_item;
  return list;
}

/*
 * curl_slist_append() appends a string to the linked list. It always returns
 * the address of the first record, so that you can use this function as an
 * initialization function as well as an append function. If you find this
 * bothersome, then simply create a separate _init function and call it
 * appropriately from within the program.
 */
struct curl_slist *curl_slist_append(struct curl_slist *list,
                                     const char *data)
{
  char *dupdata = strdup(data);

  if(!dupdata)
    return NULL;

  list = Curl_slist_append_nodup(list, dupdata);
  if(!list)
    free(dupdata);

  return list;
}

/*
 * Curl_slist_duplicate() duplicates a linked list. It always returns the
 * address of the first record of the cloned list or NULL in case of an
 * error (or if the input list was NULL).
 */
struct curl_slist *Curl_slist_duplicate(struct curl_slist *inlist)
{
  struct curl_slist *outlist = NULL;
  struct curl_slist *tmp;

  while(inlist) {
    tmp = curl_slist_append(outlist, inlist->data);

    if(!tmp) {
      curl_slist_free_all(outlist);
      return NULL;
    }

    outlist = tmp;
    inlist = inlist->next;
  }
  return outlist;
}

/* be nice and clean up resources */
void curl_slist_free_all(struct curl_slist *list)
{
  struct curl_slist     *next;
  struct curl_slist     *item;

  if(!list)
    return;

  item = list;
  do {
    next = item->next;
    Curl_safefree(item->data);
    free(item);
    item = next;
  } while(next);
}

void DL_ERROR(const char *name, int statusCode, struct dl_args *i) {
  if (!i->is_threaded)
    log_warn("[HTTP] Download Failed with code HEX: %x Int: %i from Function "
             "%s src: %s",
             statusCode, statusCode, name, i->src);
  else
    msgok(MSG_DIALOG::WARNING,
          fmt::format(
              "{0:}\n\nHEX: {1:#x} Int: {2:d}\nfrom Function {3:} src: {4:}",
              getLangSTR(LANG_STR::DL_FAILED_W), statusCode, statusCode, name,
              i->src));
}

bool (*sceStoreApiFetchJson)(const char *ua, const char *url,
                             char *json) = NULL;

bool init_curl() {

  if ((curl_global_init = (CURLcode(*)(long))prx_func_loader(
           asset_path("../Media/curl.prx"), "curl_global_init")) == NULL) {
    return false;
  }
#if 0
    if((curl_slist_append = (struct curl_slist *(*)(struct curl_slist *, const char *))prx_func_loader(asset_path("../Media/curl.prx"), "curl_slist_append")) == NULL){
       return false;
    }
    if((curl_slist_free_all = (void (*)(struct curl_slist *))prx_func_loader(asset_path("../Media/curl.prx"), "curl_slist_free_all")) == NULL){
       return false;
    }
#endif
  if ((curl_global_cleanup = (void (*)())prx_func_loader(
           asset_path("../Media/curl.prx"), "curl_global_cleanup")) == NULL) {
    return false;
  }
  if ((curl_version = (char *(*)())prx_func_loader(
           asset_path("../Media/curl.prx"), "curl_version")) == NULL) {
    return false;
  }
  if ((curl_easy_init = (CURL * (*)()) prx_func_loader(
           asset_path("../Media/curl.prx"), "curl_easy_init")) == NULL) {
    return false;
  }
  if ((curl_easy_setopt = (CURLcode(*)(CURL * curl, CURLoption option, ...))
           prx_func_loader(asset_path("../Media/curl.prx"),
                           "curl_easy_setopt")) == NULL) {
    return false;
  }
  if ((curl_easy_perform = (CURLcode(*)(CURL * curl)) prx_func_loader(
           asset_path("../Media/curl.prx"), "curl_easy_perform")) == NULL) {
    return false;
  }
  if ((curl_easy_cleanup = (void (*)(CURL *curl))prx_func_loader(
           asset_path("../Media/curl.prx"), "curl_easy_cleanup")) == NULL) {
    return false;
  }
  if ((curl_easy_getinfo =
           (CURLcode(*)(CURL * handle, CURLINFO info, long *codep))
               prx_func_loader(asset_path("../Media/curl.prx"),
                               "curl_easy_getinfo")) == NULL) {
    return false;
  }
  if ((curl_easy_strerror = (const char *(*)(CURLcode errornum))prx_func_loader(
           asset_path("../Media/curl.prx"), "curl_easy_strerror")) == NULL) {
    return false;
  }

  int ress = curl_global_init(CURL_GLOBAL_SSL);
  if (ress < 0) {
    log_info("curl_global_init failed");
    return false;
  } else {
    log_info("curl_version | %s", curl_version());
    curl_initialized = true;
    return true;
  }

  return false;
}
static int update_progress(void *p, int64_t dltotal, int64_t dlnow,
                           int64_t ultotal, int64_t ulnow) {
  // only show progress every 10mbs to avoid spamming the log
  if (dltotal > 0 && dlnow > 0 && dlnow % MB(5) == 0)
    log_info("[HTTP] Download Progress: %i/%i", dlnow, dltotal);

  return 0;
}

long curlDownloadFile(const char *url, const char *file) {
  CURL *curl = NULL;

  if (!curl_initialized) {
    log_error("curl not initialized");
    return ITEMZCORE_GENERAL_ERROR;
  }

  FILE *imageFD = fopen(file, "wb");
  if (!imageFD) {
    log_error("Failed to open file: %s | %s", file, strerror(errno));
    return ITEMZCORE_GENERAL_ERROR;
  }

  CURLcode res = (CURLcode)-1;
  log_info("Downloading: %s | %p | %p", url, curl_easy_init,
           curl_easy_strerror);
  curl = curl_easy_init();
  log_info("curl_easy_init: %p", curl);
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // Set user agent string
    std::string ua =
        fmt::format("Itemzflow GM/VER: {0:d}.{1:d} FW: {2:#x}", VERSION_MAJOR,
                    VERSION_MINOR, ps4_fw_version());
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
    if (res != CURLE_OK) {
      log_info("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    } else {
      long httpresponsecode = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpresponsecode);
      // close filedescriptor
      fclose(imageFD), imageFD = NULL;
      // cleanup
      curl_easy_cleanup(curl);
      log_info("httpresponsecode: %lu", httpresponsecode);
      return httpresponsecode;
    }

  } else {
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
int dl_from_url(const char *url_, const char *dst_) {
  long code = curlDownloadFile(url_, dst_);
  if ((int)code != 200) {
    log_info("curlDownloadFile failed: %d", code);
    return (int)code;
  } else
    log_info("curlDownloadFile success");

  return 0;
}
#define BUFFER_SIZE (0x10000) /* 256kB */

using json = nlohmann::json;

struct write_result {
  char *data;
  int pos;
};

static size_t curl_write(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct write_result *result = (struct write_result *)stream;

  /* Will we overflow on this write? */
  if (result->pos + size * nmemb >= BUFFER_SIZE - 1) {
    log_error("curl error: too small buffer\n");
    return 0;
  }

  /* Copy curl's stream buffer into our own buffer */
  memcpy(result->data + result->pos, ptr, size * nmemb);

  /* Advance the position */
  result->pos += size * nmemb;

  return size * nmemb;
}

std::string check_from_url(std::string &url_) {
  char JSON[BUFFER_SIZE];
  CURL *curl;
  CURLcode res;
  long http_code = 0;

  memset(JSON, 0, sizeof(JSON));

  if (!curl_initialized) {
    log_error("curl not initialized");
    return std::string();
  }

  std::string ua = fmt::format("Itemzflow GM/VER: {0:d}.{1:d} FW: {2:#x}",
                               VERSION_MAJOR, VERSION_MINOR, ps4_fw_version());
  curl = curl_easy_init();
  if (curl) {
    struct write_result write_result = {.data = JSON, .pos = 0};
    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    res = curl_easy_perform(curl);
    if (!res) {
      log_info("RESPONSE: %s", JSON);
    } else {
      log_warn("[StoreCore][HTTP] curl_easy_perform() failed with code %i from "
               "Function %s url: %s : error: %s",
               res, "ini_dl_req", url_.c_str(), curl_easy_strerror(res));
      curl_easy_cleanup(curl);
      return std::string();
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
      curl_easy_cleanup(curl);
      return std::string();
    }
    curl_easy_cleanup(curl);
  } else {
    log_warn("[StoreCore][HTTP] curl_easy_perform() failed Function %s url: %s",
             "ini_dl_req", url_.c_str());
    return std::string();
  }

    auto j = nlohmann::json::parse(JSON, nullptr, false);
    
    // Check if the parsing was successful
    if (j.is_discarded()) {
        log_error("Could not parse update json");
        return std::string();
    }
    
    // Check if the "hash" key exists and is a string
    if (j.contains("hash") && j["hash"].is_string()) {
        return j["hash"].get<std::string>();
    } else {
        log_error("Update format not found");
        return std::string();
    }

  return std::string();
}

bool fetch_json(const std::string &url, std::string &readBuffer, bool &is_connection_issue) {
  CURL *curl;
  CURLcode res;
  char JSON[BUFFER_SIZE];
  long http_code;
  memset(JSON, 0, sizeof JSON);
  struct write_result write_result = {.data = JSON, .pos = 0};

  curl = curl_easy_init();
  if (curl) {
    std::string ua =
        fmt::format("Itemzflow GM/VER: {0:d}.{1:d} FW: {2:#x}", VERSION_MAJOR,
                    VERSION_MINOR, ps4_fw_version());
    log_info("useragent: %s", ua.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());

    res = curl_easy_perform(curl);
    log_info("%s", JSON);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if(http_code != 404)
       is_connection_issue = true;

    curl_easy_cleanup(curl);
    readBuffer = std::string(JSON);

    return res == CURLE_OK;
  }
  return false;
}

bool Fetch_Update_Details(const std::string &title_id, const std::string &title,
                          _update_struct &updateStruct,
                          bool &is_connection_issue) {
  std::string url = PATCH_API_URL + title_id;
  std::string readBuffer;
  // clear the whole struct
  updateStruct.update_title.clear();
  updateStruct.update_tid.clear();
  updateStruct.update_version.clear();
  updateStruct.update_size.clear();
  updateStruct.update_json.clear();
  updateStruct.required_fw.clear();

  // log_info("Fetch_Update_Details: %s", url.c_str());
  if (!fetch_json(url, readBuffer, is_connection_issue)) {
    log_error("Failed to fetch update details for %s", title.c_str());
    return false;
  }

  // Parse the JSON
  auto json = nlohmann::json::parse(readBuffer, nullptr, false);

  // Check if the parsing was successful
  if (json.is_discarded()) {
    log_error("Failed to parse update json");
    return false;
  }

  log_info("Json: %s", json.dump().c_str());
  updateStruct.update_title = title;
  updateStruct.update_tid = title_id;

  // Check if "patches" key exists and is an array
  if (json.contains("patches") && json["patches"].is_array()) {
    for (const auto &patch : json["patches"]) {
      if (patch.contains("version") && patch["version"].is_string() &&
          patch.contains("size") && patch["size"].is_number() &&
          patch.contains("endpoint") && patch["endpoint"].is_string()) {
        updateStruct.update_version.push_back(
            patch["version"].get<std::string>());
        updateStruct.update_size.push_back(
            std::to_string(patch["size"].get<size_t>()));
        updateStruct.update_json.push_back(
            patch["endpoint"].get<std::string>());
        // required_fw
        if (patch.contains("required_fw") && patch["required_fw"].is_number())
          updateStruct.required_fw.push_back(patch["required_fw"].get<int>());
        else
          updateStruct.required_fw.push_back(0);


      } else {
        log_error("Invalid patch format in json");
        return false;
      }
    }
  } else {
    log_error("No patches found or patches is not an array");
    return false;
  }

  return updateStruct.update_json.size() > 0;
}