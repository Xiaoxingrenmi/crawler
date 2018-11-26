
// Socket components for HTTP
//   by BOT Man & ZhangHan, 2018

#include "http_socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define RECV_BUFFER_SIZE 64

#define CONTENT_LENGTH_START "Content-Length: "
#define CONTENT_LENGTH_TEMPLATE "Content-Length: %lu\r"

#define CONTENT_START "\r\n\r\n"

int InitSocket(const char* host, uint16_t port) {
  // create handle
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET)
    return INVALID_SOCKET;

  // init sockaddr_in struct with |port|
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);

  // check |host| type (domain name or ip address)
  struct hostent* host_by_name = gethostbyname(host);
  if (host_by_name && host_by_name->h_addrtype == AF_INET) {
    // |host| is domain name
    sa.sin_addr.s_addr = *(in_addr_t*)host_by_name->h_addr_list[0];
  } else {
    // |host| is ip address
    inet_pton(sa.sin_family, host, &sa.sin_addr);
  }

  // connect to |ip:port|
  if (-1 == connect(sock, (struct sockaddr*)&sa, sizeof sa)) {
    close(sock);
    return INVALID_SOCKET;
  }

  // return handle
  return sock;
}

ssize_t SendHttpRequest(int sock, const char* data) {
  // send |buffer| and return length sent
  // (-1 if server close the connection)
  return send(sock, data, strlen(data), 0);
}

char* RecvHttpResponse(int sock) {
  char* recv_buffer = NULL;
  size_t content_len = 0;

  // fill |recv_buffer|
  while (1) {
    // recv to |buffer|
    char buffer[RECV_BUFFER_SIZE];
    ssize_t len = recv(sock, buffer, RECV_BUFFER_SIZE - 1, 0);
    buffer[len] = 0;

    // server close the connection
    if (-1 == len) {
      if (recv_buffer)
        free(recv_buffer);
      return NULL;
    }

    // no more data to recv
    if (0 == len)
      break;

    // allocate enough memory to |recv_buffer|
    size_t previous_len = 0;
    if (!recv_buffer) {
      // init |recv_buffer| with malloc
      recv_buffer = malloc((size_t)len + 1);
    } else {
      // update |previous_len|
      previous_len = strlen(recv_buffer);

      // realloc memory of |recv_buffer|
      recv_buffer = realloc(recv_buffer, previous_len + (size_t)len + 1);
    }

    // make |recv_buffer| string (ending with 0)
    recv_buffer[previous_len] = 0;

    // append |buffer| to |recv_buffer|
    strncat(recv_buffer, buffer, (size_t)len);

    // parse content-length from |recv_buffer|
    if (!content_len) {
      char* cont_len_str = strstr(recv_buffer, CONTENT_LENGTH_START);
      if (cont_len_str) {
        size_t val = 0;
        if (EOF != sscanf(cont_len_str, CONTENT_LENGTH_TEMPLATE, &val))
          content_len = val;
      }
    }

    // check length of content
    if (content_len) {
      char* cont_str = strstr(recv_buffer, CONTENT_START);
      if (cont_str) {
        // move to the start of content data
        cont_str += sizeof CONTENT_START - 1;
        if (cont_str && strlen(cont_str) >= content_len) {
          break;
        }
      }
    }
  }

  // return |recv_buffer|
  return recv_buffer;
}

void CloseSocket(int sock) {
  if (sock == INVALID_SOCKET)
    return;

  // shutdown socket connection
  shutdown(sock, SHUT_RDWR);

  // close handle
  close(sock);
}
