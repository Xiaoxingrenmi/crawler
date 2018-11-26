
// Naive http client
//   by BOT Man & ZhangHan, 2018

#include "http_client.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_socket.h"
#include "string_helper.h"

#define SEND_BUFFER_SIZE 512

#define HTTP_GET_TEMPLATE \
  "GET %s HTTP/1.1\r\n\
Host: %s\r\n\
User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.102 Safari/537.36\r\n\
Accept: text/html,application/xhtml+xml,application/xml\r\n\
\r\n\
"

#define CONTENT_START "\r\n\r\n"

#define URL_HTTP_SCHEME_START "http://"
#define URL_PATH_START "/"

char* ParseHost(const char* url) {
  // find the begin of host
  const char* host_beg = NULL;
  if (strstr(url, URL_HTTP_SCHEME_START) == url) {
    // url is like 'http://github.com'
    host_beg = url + sizeof URL_HTTP_SCHEME_START - 1;
  } else {
    // url is like 'github.com'
    host_beg = url;
  }

  // find the end of host
  const char* host_end = strstr(host_beg, URL_PATH_START);
  if (!host_end) {
    // url has no explicit path piece
    host_end = url + strlen(url);
  }

  return CopyrString(host_beg, host_end);
}

char* ParseRequest(const char* url) {
  // find the begin of host
  const char* host_beg = NULL;
  if (strstr(url, URL_HTTP_SCHEME_START) == url) {
    // url is like 'http://github.com'
    host_beg = url + sizeof URL_HTTP_SCHEME_START - 1;
  } else {
    // url is like 'github.com'
    host_beg = url;
  }

  // find the begin of request
  const char* request_beg = strstr(host_beg, URL_PATH_START);
  if (!request_beg) {
    // url has no path piece
    return CopyString("/");
  } else {
    // url has path piece
    return CopyString(request_beg);
  }
}

void Request(const char* url, request_callback_fn callback, void* context) {
  char* host = ParseHost(url);
  char* request = ParseRequest(url);

  int sock = InitSocket(host, 80);
  if (sock == INVALID_SOCKET) {
    free(host);
    free(request);

    callback(NULL, NULL, context);
    return;
  }

  char req_data[SEND_BUFFER_SIZE];
  sprintf(req_data, HTTP_GET_TEMPLATE, request, host);
  if (-1 == SendHttpRequest(sock, req_data)) {
    free(host);
    free(request);
    CloseSocket(sock);

    callback(NULL, NULL, context);
    return;
  }

  char* response = RecvHttpResponse(sock);
  const char* html = strstr(response, CONTENT_START);
  if (html) {
    html += sizeof CONTENT_START - 1;
  }
  callback(response, html, context);
  free(response);

  free(host);
  free(request);
  CloseSocket(sock);
}
