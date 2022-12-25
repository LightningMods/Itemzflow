#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <curl/curl.h>

int curlDownloadFile(const char * url,
  const char * file,
    const char * ua,
      int( * update_cb)(void * p, int64_t dltotal, int64_t dlnow, int64_t ultotal, int64_t ulnow)) {
  CURL * curl = NULL;
  FILE * imageFD = fopen(file, "wb");
  if (!imageFD) {
    return -1336;
  }

  CURLcode res;
  if (!curl) curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // Set user agent string
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua);
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
    // Fail the request if the HTTP code returned is equal to or larger than 400
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    // request using SSL for the FTP transfer if available
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);

    /* pass the struct pointer into the xferinfo function */
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, update_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      return -res;

    } else {
      int httpresponsecode = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, & httpresponsecode);
      // close filedescriptor
      fclose(imageFD);
      // cleanup
      curl_easy_cleanup(curl);
      return httpresponsecode;
    }

  } else {
    printf("NO CURL curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
  }

  // close filedescriptor
  fclose(imageFD);

  // cleanup
  curl_easy_cleanup(curl);

  return -1337;

}