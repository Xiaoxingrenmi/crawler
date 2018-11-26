
// Parse urls from html code
//   by BOT Man & ZhangHan, 2018

#include "html_parser.h"

#include <stdlib.h>
#include "string_helper.h"

void ParseAtagUrls(const char* html,
                   yeild_atag_urls_callback_fn callback,
                   void* context) {
  int state = 0;
  const char* url_beg = NULL;
  const char* url_end = NULL;

  for (const char* p = html; *p != '\0'; p++) {
    switch (state) {
      case 0:
        if (*p == '<')
          state = 1;
        else
          state = 0;
        break;

      case 1:
        if (*p == 'a')
          state = 2;
        else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
          state = 1;
        else
          state = 0;
        break;

      case 2:
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
          state = 3;
        else
          state = 0;
        break;

      case 3:
        if (*p == 'h')
          state = 4;
        else if (*p == '>')
          state = 0;
        else
          state = 3;
        break;

      case 4:
        if (*p == 'r')
          state = 5;
        else if (*p == 'h')
          state = 4;
        else if (*p == '>')
          state = 0;
        else
          state = 3;
        break;

      case 5:
        if (*p == 'e')
          state = 6;
        else if (*p == 'h')
          state = 4;
        else if (*p == '>')
          state = 0;
        else
          state = 3;
        break;

      case 6:
        if (*p == 'f')
          state = 7;
        else if (*p == 'h')
          state = 4;
        else if (*p == '>')
          state = 0;
        else
          state = 3;
        break;

      case 7:
        if (*p == '=')
          state = 8;
        else if (*p == 'h')
          state = 4;
        else if (*p == '>')
          state = 0;
        else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
          state = 7;
        else
          state = 3;
        break;

      case 8:
        if (*p == '"') {
          state = 9;
          url_beg = p + 1;
        } else if (*p == 'h')
          state = 4;
        else if (*p == '>')
          state = 0;
        else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
          state = 8;
        else
          state = 3;
        break;

      case 9:
        if (*p == '"') {
          state = 10;
          url_end = p;
        } else
          state = 9;
        break;

      case 10:
        if (*p == '>') {
          state = 0;

          char* url = CopyrString(url_beg, url_end);
          if (url) {
            callback(url, context);
            free(url);
          }

          url_beg = NULL;
          url_end = NULL;
        } else
          state = 10;
        break;
    }
  }
}
