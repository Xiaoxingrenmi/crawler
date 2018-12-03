
// Common String Helpers
//   by BOT Man & ZhangHan, 2018

#ifndef STRING_HELPER
#define STRING_HELPER

#include <stddef.h>

char* CopyString(const char* src);
char* CopyrString(const char* beg, const char* end);
char* CopynString(const char* src, size_t len);

char* ConcatString(const char* a, const char* b);
char* ConcatOwnedString(char* a, char* b);  // take the ownership of |a| and |b|

#endif  // STRING_HELPER
