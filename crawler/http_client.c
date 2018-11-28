
// Naive http client
//   by BOT Man & ZhangHan, 2018

#include "http_client.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// For inet_pton
#include <arpa/inet.h>
// For libevent functions
#include <event2/event.h>
// For sockaddr_in
#include <netinet/in.h>
// For socket functions
#include <sys/socket.h>

#include "string_helper.h"

#define SEND_BUFFER_SIZE 512
#define RECV_BUFFER_SIZE 64

#define HTTP_GET_TEMPLATE \
  "GET %s HTTP/1.1\r\n\
Host: %s\r\n\
User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.102 Safari/537.36\r\n\
Accept: text/html,application/xhtml+xml,application/xml\r\n\
\r\n\
"

#define CONTENT_LENGTH_START "Content-Length: "
#define CONTENT_LENGTH_TEMPLATE "Content-Length: %lu\r"
#define CONTENT_START "\r\n\r\n"

#define URL_HTTP_SCHEME_START "http://"
#define URL_HTTPS_SCHEME_START "https://"
#define URL_PATH_START "/"

char* ParseHost(const char* url) {
  if (!url)
    return NULL;

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
  if (!url)
    return NULL;

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

char* ConstructSendBuffer(const char* url) {
  if (!url)
    return NULL;

  char* host = ParseHost(url);
  char* request = ParseRequest(url);

  char buffer[SEND_BUFFER_SIZE];
  sprintf(buffer, HTTP_GET_TEMPLATE, request, host);

  free((void*)host);
  free((void*)request);

  return CopyString(buffer);
}

struct sockaddr_in ConstructSockAddr(const char* host, uint16_t port) {
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

  return sa;
}

evutil_socket_t CreateSocket(struct sockaddr_in* sa) {
  // create handle
  evutil_socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
    return -1;

  // connect to |ip:port|
  if (connect(sock, (struct sockaddr*)sa, sizeof(struct sockaddr_in)) < 0) {
    EVUTIL_CLOSESOCKET(sock);
    return -1;
  }

  return sock;
}

typedef struct {
  request_callback_fn callback;
  void* context;

  struct event* send_event;
  char* send_buffer;
  size_t n_sent;

  struct event* recv_event;
  char* recv_buffer;
  size_t n_written;
  size_t content_length;
} RequestState;

size_t g_request_state_count;

void DoSend(evutil_socket_t fd, short events, void* context);
void DoRecv(evutil_socket_t fd, short events, void* context);

void FreeState(RequestState* state) {
  if (!state)
    return;

  if (state->send_event)
    event_free(state->send_event);
  if (state->recv_event)
    event_free(state->recv_event);

  if (state->send_buffer)
    free((void*)state->send_buffer);
  if (state->recv_buffer)
    free((void*)state->recv_buffer);

  --g_request_state_count;
  free((void*)state);
}

RequestState* CreateState(struct event_base* base,
                          evutil_socket_t fd,
                          const char* url,
                          request_callback_fn callback,
                          void* context) {
  RequestState* ret = (RequestState*)malloc(sizeof(RequestState));
  assert(ret);
  if (!ret)
    return NULL;
  memset(ret, 0, sizeof(RequestState));
  ++g_request_state_count;

  ret->send_event = event_new(base, fd, EV_WRITE | EV_PERSIST, DoSend, ret);
  if (!ret->send_event) {
    FreeState(ret);
    return NULL;
  }

  ret->recv_event = event_new(base, fd, EV_READ | EV_PERSIST, DoRecv, ret);
  if (!ret->recv_event) {
    FreeState(ret);
    return NULL;
  }

  ret->send_buffer = ConstructSendBuffer(url);
  if (!ret->send_buffer) {
    FreeState(ret);
    return NULL;
  }

  ret->callback = callback;
  ret->context = context;
  return ret;
}

void DoSend(evutil_socket_t fd, short events, void* context) {
  assert(context);
  RequestState* state = (RequestState*)context;

  size_t write_upto = strlen(state->send_buffer);
  while (state->n_written < write_upto) {
    ssize_t result = send(fd, state->send_buffer + state->n_written,
                          write_upto - state->n_written, 0);
    if (result < 0) {
      // continue in next term
      if (EVUTIL_SOCKET_ERROR() == EAGAIN)
        return;

      // failed callback
      state->callback(NULL, NULL, state->context);
      FreeState(state);
      return;
    }
    assert(result != 0);

    // continue sending
    state->n_written += (size_t)result;
  }

  // finish sending
  assert(state->n_written == write_upto);

  // stop reading and start recving
  event_del(state->send_event);
  event_add(state->recv_event, NULL);
}

void DoRecv(evutil_socket_t fd, short events, void* context) {
  assert(context);
  RequestState* state = (RequestState*)context;

  char buffer[RECV_BUFFER_SIZE];
  while (1) {
    ssize_t result = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (result < 0) {
      // continue in next term
      if (EVUTIL_SOCKET_ERROR() == EAGAIN)
        return;

      // failed callback
      state->callback(NULL, NULL, state->context);
      FreeState(state);
      return;
    } else if (result == 0) {
      // succeeded callback
      break;
    }

    // continue recving
    {
      // allocate/reallocate memory of |recv_buffer|
      size_t previous_len = 0;
      if (!state->recv_buffer) {
        // init |recv_buffer| with malloc
        state->recv_buffer = (char*)malloc((size_t)result + 1);
      } else {
        // update |previous_len|
        previous_len = strlen(state->recv_buffer);

        // realloc memory of |recv_buffer|
        state->recv_buffer = (char*)realloc(state->recv_buffer,
                                            previous_len + (size_t)result + 1);
      }
      assert(state->recv_buffer);

      // make |recv_buffer| C-style string
      state->recv_buffer[previous_len] = 0;

      // append |buffer| to |recv_buffer|
      strncat(state->recv_buffer, buffer, (size_t)result);
    }

    // process content length
    {
      // try parse 'Content-Length:' from |recv_buffer|
      if (!state->content_length) {
        const char* cont_len_str =
            strstr(state->recv_buffer, CONTENT_LENGTH_START);
        if (cont_len_str) {
          size_t val = 0;
          if (EOF != sscanf(cont_len_str, CONTENT_LENGTH_TEMPLATE, &val))
            state->content_length = val;
        }
      }

      // check length of content
      if (state->content_length) {
        const char* cont_str = strstr(state->recv_buffer, CONTENT_START);
        if (cont_str) {
          // move to the start of content data
          cont_str += sizeof CONTENT_START - 1;

          // check if recv sufficient data
          if (cont_str && strlen(cont_str) >= state->content_length) {
            assert(strlen(cont_str) == state->content_length);

            // succeeded callback
            break;
          }
        }
      }
    }
  }

  // succeeded callback
  const char* html = strstr(state->recv_buffer, CONTENT_START);
  if (html) {
    html += sizeof CONTENT_START - 1;
  }
  state->callback(state->recv_buffer, html, state->context);

  // stop recving and delete context
  event_del(state->recv_event);
  FreeState(state);

  // close socket
  shutdown(fd, SHUT_RDWR);
  EVUTIL_CLOSESOCKET(fd);
}

struct event_base* g_event_base;

void Request(const char* url, request_callback_fn callback, void* context) {
  assert(url);

  if (!g_event_base)
  {
    g_event_base = event_base_new();
    assert(g_event_base);
  }

  // ignore https url
  if (strstr(url, URL_HTTPS_SCHEME_START)) {
    callback(NULL, NULL, context);
    return;
  }

  char* host = ParseHost(url);
  if (!host) {
    callback(NULL, NULL, context);
    return;
  }

  struct sockaddr_in sa = ConstructSockAddr(host, 80);
  evutil_socket_t fd = CreateSocket(&sa);

  free((void*)host);

  if (fd < 0) {
    callback(NULL, NULL, context);
    return;
  }

  RequestState* state = CreateState(g_event_base, fd, url, callback, context);
  if (!state) {
    EVUTIL_CLOSESOCKET(fd);
    callback(NULL, NULL, context);
    return;
  }

  evutil_make_socket_nonblocking(fd);
  event_add(state->send_event, NULL);
}

void DispatchLibEvent() {
  assert(g_event_base);
  event_base_dispatch(g_event_base);

  event_base_free(g_event_base);
  assert(g_request_state_count == 0);
}
