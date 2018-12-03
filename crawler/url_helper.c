
// Url Helpers
//   by BOT Man & ZhangHan, 2018

#include "url_helper.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "string_helper.h"

#define URL_HTTP_SCHEME_START "http://"
#define URL_HTTPS_SCHEME_START "https://"
#define URL_FILE_SCHEME_START "file://"
#define URL_FTP_SCHEME_START "ftp://"
#define URL_PATH_START "/"
#define URL_QUERY_START "?"
#define URL_FRAGMENT_START "#"
#define URL_PATH_UP "../"
#define URL_PATH_CHAR '/'

const char* FindPathEnd(const char* path_beg) {
  // /BOT-Man-JL?q -> ^?q
  const char* query_beg = strstr(path_beg, URL_QUERY_START);
  if (!query_beg) {
    // /BOT-Man-JL -> /BOT-Man-JL^
    query_beg = path_beg + strlen(path_beg);
  }

  // /BOT-Man-JL#f -> ^#f
  const char* fragment_beg = strstr(path_beg, URL_FRAGMENT_START);
  if (!fragment_beg) {
    // /BOT-Man-JL -> /BOT-Man-JL^
    fragment_beg = path_beg + strlen(path_beg);
  }

  // /BOT-Man-JL?q -> /BOT-Man-JL^
  // /BOT-Man-JL#f -> /BOT-Man-JL^
  // /BOT-Man-JL?q#f -> /BOT-Man-JL^
  return (query_beg < fragment_beg) ? query_beg : fragment_beg;
}

// find last slash between [path_beg, path_end)
const char* FindPathLastSlash(const char* path_beg, const char* path_end) {
  assert(path_beg < path_end);

  const char* last_slash = path_end - 1;
  while (*last_slash != URL_PATH_CHAR && last_slash != path_beg)
    --last_slash;

  assert(path_beg <= last_slash);
  return last_slash;
}

// reference: HTML File Paths @ w3schools
// https://www.w3schools.com/html/html_filepaths.asp
char* FixUrl(const char* raw, const char* refer) {
  if (!raw)
    return NULL;

  // Case: |raw| is https/file/ftp url -> ignore
  if (strstr(raw, URL_HTTPS_SCHEME_START) == raw ||
      strstr(raw, URL_FILE_SCHEME_START) == raw ||
      strstr(raw, URL_FTP_SCHEME_START) == raw)
    return NULL;

  // Case: |raw| is http url -> return |raw_host| + |raw_path|
  if (strstr(raw, URL_HTTP_SCHEME_START) == raw) {
    // http://github.com -> ^github.com
    // http://github.com/BOT-Man-JL -> ^github.com/BOT-Man-JL
    const char* host_beg = raw + sizeof URL_HTTP_SCHEME_START - 1;

    // github.com/BOT-Man-JL -> github.com^
    const char* host_end = strstr(host_beg, URL_PATH_START);
    if (!host_end) {
      // github.com -> github.com^
      host_end = host_beg + strlen(host_beg);
    }

    // /BOT-Man-JL -> ^/BOT-Man-JL
    const char* path_beg = strstr(host_end, URL_PATH_START);
    if (!path_beg) {
      // "" -> "/"
      assert(0 == strlen(host_end));
      path_beg = "/";
    }

    // /BOT-Man-JL?q#f -> /BOT-Man-JL^
    const char* path_end = FindPathEnd(path_beg);
    assert(path_end);

    // return |raw_host| + |raw_path|
    return ConcatOwnedString(CopyrString(host_beg, host_end),
                             CopyrString(path_beg, path_end));
  }

  // |refer| is required below
  if (!refer)
    return NULL;

  // Case: |raw| is "/url" -> return |refer_host| + |raw_path|
  if (strstr(raw, URL_PATH_START) == raw) {
    // /BOT-Man-JL -> ^/BOT-Man-JL
    const char* path_beg = raw;

    // /BOT-Man-JL?q#f -> /BOT-Man-JL^
    const char* path_end = FindPathEnd(path_beg);
    assert(path_end);

    // return |refer_host| + |raw_path|
    return ConcatOwnedString(ParseHost(refer), CopyrString(path_beg, path_end));
  }

  // Case: |raw| is "url" or "../url"
  //        -> return |refer_host| + |refer_path_base| + |raw_path|
  {
    // /BOT-Man-JL -> ^/BOT-Man-JL
    const char* path_beg = raw;

    // calc "../"
    size_t n_path_up = 0;

    // ../../BOT-Man-JL -> BOT-Man-JL + 2
    while (strstr(path_beg, URL_PATH_UP) == path_beg) {
      path_beg += sizeof URL_PATH_UP - 1;
      ++n_path_up;
    }

    // /BOT-Man-JL?q#f -> /BOT-Man-JL^
    const char* path_end = FindPathEnd(path_beg);
    assert(path_end);

    // parse refer path
    char* refer_path = ParsePath(refer);
    if (!refer_path)
      return NULL;
    assert(strlen(refer_path) && *refer_path == URL_PATH_CHAR);

    // / -> /
    // /BOT-Man-JL/ -> /BOT-Man-JL/
    // /BOT-Man-JL/crawler -> /BOT-Man-JL/
    const char* refer_path_beg = refer_path;
    const char* refer_path_end =
        FindPathLastSlash(refer_path, refer_path + strlen(refer_path)) + 1;
    assert(refer_path_beg < refer_path_end);

    // /Path1/Path2/ + 0 -> /Path1/
    // /Path1/Path2/ + 1 -> /
    // /Path1/Path2/ + 2 -> /
    for (size_t i = 0; i < n_path_up; ++i) {
      // /Path1/Path2/ -> /Path1/Path2^/
      const char* trailing_slash = refer_path_end - 1;

      // break if |trailing_slash| == |refer_path_beg| (i.e. only "/" remaining)
      if (trailing_slash == refer_path_beg)
        break;

      // /Path1/Path2^/ -> /Path1/^Path2/ -> /Path1/
      refer_path_end = FindPathLastSlash(refer_path_beg, trailing_slash) + 1;
    }
    assert(refer_path_beg < refer_path_end);

    char* refer_path_base = CopyrString(refer_path_beg, refer_path_end);
    free((void*)refer_path);

    char* new_path =
        ConcatOwnedString(refer_path_base, CopyrString(path_beg, path_end));
    return ConcatOwnedString(ParseHost(refer), new_path);
  }
}

char* ParseHost(const char* url) {
  if (!url)
    return NULL;

  const char* host_beg = url;
  const char* host_end = strstr(url, URL_PATH_START);
  assert(host_end);

  return CopyrString(host_beg, host_end);
}

char* ParsePath(const char* url) {
  if (!url)
    return NULL;

  const char* request_beg = strstr(url, URL_PATH_START);
  assert(request_beg);

  return CopyString(request_beg);
}

uint16_t ParsePort(const char* url) {
  if (!url)
    return 0;

  // TODO: consider port in |url|
  return 80;
}
