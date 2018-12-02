
// Common String Helpers
//   by BOT Man & ZhangHan, 2018

#include "string_helper.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

char* CopyString(const char* src) {
  return CopynString(src, strlen(src));
}

char* CopyrString(const char* beg, const char* end) {
  return CopynString(beg, (size_t)(end - beg));
}

char* CopynString(const char* src, size_t len) {
  char* ret = (char*)malloc(len + 1);
  assert(ret);
  if (!ret)
    return NULL;

  strncpy(ret, src, len);
  ret[len] = 0;

  return ret;
}

char* ConcatString(const char* a, const char* b) {
  size_t len = strlen(a) + strlen(b);
  char* ret = (char*)malloc(len + 1);
  assert(ret);
  if (!ret)
    return NULL;
  memset(ret, 0, len + 1);

  strcat(ret, a);
  strcat(ret, b);
  return ret;
}
