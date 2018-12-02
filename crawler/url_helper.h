
// Url Helpers
//   by BOT Man & ZhangHan, 2018

#ifndef URL_HELPER
#define URL_HELPER

#include <stdint.h>

char* FixUrl(const char* raw, const char* refer);

char* ParseHost(const char* url);

char* ParseRequest(const char* url);

uint16_t ParsePort(const char* url);

#endif  // URL_HELPER
