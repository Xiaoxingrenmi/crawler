
// Socket components for HTTP
//   by BOT Man & ZhangHan, 2018

#ifndef HTTP_SOCKET
#define HTTP_SOCKET

#include <stdint.h>
#include <unistd.h>

#define INVALID_SOCKET -1

int InitSocket(const char* host, uint16_t port);

ssize_t SendHttpRequest(int sock, const char* data);

char* RecvHttpResponse(int sock);

void CloseSocket(int sock);

#endif  // HTTP_SOCKET
