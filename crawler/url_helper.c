
// Url Helpers
//   by BOT Man & ZhangHan, 2018

#include "url_helper.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "string_helper.h"

#define URL_HTTP_SCHEME_START "http://"
#define URL_HTTPS_SCHEME_START "https://"
#define URL_PATH_START "/"
#define URL_QUERY_START "?"
#define URL_FRAGMENT_START "#"

char* FixUrl(const char* raw, const char* refer) {
  if (!raw)
    return NULL;

  // |raw| is https url -> ignore
  if (strstr(raw, URL_HTTPS_SCHEME_START))
    return NULL;

  // |raw| is http url -> return |host| + |path|
  if (strstr(raw, URL_HTTP_SCHEME_START) == raw) {
    // http://github.com -> ^github.com
    // http://github.com/BOT-Man-JL -> ^github.com/BOT-Man-JL
    const char* host_beg = raw + sizeof URL_HTTP_SCHEME_START - 1;

    // github.com/BOT-Man-JL -> github.com^
    const char* host_end = strstr(host_beg, URL_PATH_START);
    if (!host_end) {
      // github.com -> github.com^
      host_end = raw + strlen(raw);
    }

    // /BOT-Man-JL -> ^/BOT-Man-JL
    const char* request_beg = strstr(host_end, URL_PATH_START);
    if (!request_beg) {
      // "" -> "/"
      assert(0 == strlen(host_end));
      request_beg = "/";
    }

    const char* query_beg = strstr(request_beg, URL_QUERY_START);
    if (!query_beg)
      query_beg = raw + strlen(raw);

    const char* fragment_beg = strstr(request_beg, URL_FRAGMENT_START);
    if (!fragment_beg)
      fragment_beg = raw + strlen(raw);

    const char* request_end =
        query_beg < fragment_beg ? query_beg : fragment_beg;

    char* ret = NULL;
    char* host_piece = CopyrString(host_beg, host_end);
    char* request_piece = CopyrString(request_beg, request_end);

    if (host_piece && request_piece) {
      ret = ConcatString(host_piece, request_piece);
    }

    if (host_piece)
      free((void*)host_piece);
    if (request_piece)
      free((void*)request_piece);
    return ret;
  }

  // |raw| is relative url -> return |refer| + |raw|
  // TODO: handle relative url
  return NULL;
}

char* ParseHost(const char* url) {
  if (!url)
    return NULL;

  const char* host_end = strstr(url, URL_PATH_START);
  assert(host_end);

  return CopyrString(url, host_end);
}

char* ParseRequest(const char* url) {
  if (!url)
    return NULL;

  const char* request_beg = strstr(url, URL_PATH_START);
  assert(request_beg);

  return CopyString(request_beg);
}

uint16_t ParsePort(const char* url) {
  if (!url)
    return 0;

  // TOOD: consider port in |url|
  return 80;
}
