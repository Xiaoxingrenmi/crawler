
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
#include "url_helper.h"

#define CONN_TIMEOUT_SEC 5
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

//
// url helpers
//

char* ConstructSendBuffer(const char* url) {
  char* ret = NULL;
  char* host = ParseHost(url);
  char* path = ParsePath(url);

  if (host && path) {
    char buffer[SEND_BUFFER_SIZE];
    sprintf(buffer, HTTP_GET_TEMPLATE, path, host);
    ret = CopyString(buffer);
  }

  if (host)
    free((void*)host);
  if (path)
    free((void*)path);
  return ret;
}

struct sockaddr_in ConstructSockAddr(const char* url) {
  struct sockaddr_in ret = {0};
  char* host = ParseHost(url);
  uint16_t port = ParsePort(url);

  if (host && port) {
    ret.sin_family = AF_INET;
    ret.sin_port = htons(port);

    // check |host| type (domain name or ip address)
    struct hostent* host_by_name = gethostbyname(host);
    if (host_by_name && host_by_name->h_addrtype == AF_INET) {
      // |host| is domain name
      ret.sin_addr.s_addr = *(in_addr_t*)host_by_name->h_addr_list[0];
    } else {
      // |host| is ip address
      inet_pton(ret.sin_family, host, (void*)&ret.sin_addr);
    }
  }

  if (host)
    free((void*)host);
  return ret;
}

//
// state definitions
//

typedef struct {
  // requested url
  char* url;

  // callback data
  request_callback_fn callback;
  void* context;

  // event/buffer of current state
  struct event* event;
  char* buffer;

  // event specific data
  union {
    // used to track sent data
    size_t n_sent;

    // used to check recv data
    size_t content_length;
  };
} RequestState;

size_t g_request_state_count;

RequestState* CreateState(const char* url,
                          request_callback_fn callback,
                          void* context) {
  RequestState* ret = (RequestState*)malloc(sizeof(RequestState));
  if (!ret)
    return NULL;
  memset(ret, 0, sizeof(RequestState));

  ret->url = CopyString(url);
  if (!ret->url) {
    free((void*)ret);
    return NULL;
  }

  ret->callback = callback;
  ret->context = context;

  ++g_request_state_count;
  return ret;
}

void FreeState(RequestState* state) {
  assert(state);
  assert(state->url);

  free((void*)state->url);
  free((void*)state);

  --g_request_state_count;
}

//
// trans-state helpers
//

typedef enum {
  MaybeFree,    // free previous state's event/buffer if exists
  RequireFree,  // require free previous state's event/buffer (assert non-empty)
  DontFree,     // don't free previous state's event/buffer (assert empty)
} TransformAction;

void TransformStateEvent(RequestState* state,
                         struct event* new_event,
                         TransformAction action) {
  assert(state);

  switch (action) {
    case RequireFree:
      assert(state->event);
      break;
    case DontFree:
      assert(!state->event);
      break;
    case MaybeFree:
    default:
      // don't check
      break;
  }

  if (state->event)
    event_free(state->event);
  state->event = new_event;
}

void TransformStateBuffer(RequestState* state,
                          char* new_buffer,
                          TransformAction action) {
  assert(state);

  switch (action) {
    case RequireFree:
      assert(state->buffer);
      break;
    case DontFree:
      assert(!state->buffer);
      break;
    case MaybeFree:
    default:
      // don't check
      break;
  }

  if (state->buffer)
    free((void*)state->buffer);
  state->buffer = new_buffer;
}

/*
  State Transformation:

                     DoInit   DoConn   DoSend   DoRecv
                        \        \        \        \
 (CreateState) --> Init --> Conn --> Send --> Recv --> Succ --:
                    :        :        :        :              :--> (FreeState)
                    :--------:--------:--------:-----> Fail --:
*/

// trans-state functions

void StateInitToConn(evutil_socket_t fd, RequestState* state);
void StateConnToSend(evutil_socket_t fd, RequestState* state);
void StateSendToRecv(evutil_socket_t fd, RequestState* state);
void StateRecvToSucc(evutil_socket_t fd, RequestState* state);
void StateToFail(evutil_socket_t fd, RequestState* state, RequestStatus status);

// in-state functions

void DoInit(evutil_socket_t fd, short events, void* context);
void DoConn(evutil_socket_t fd, short events, void* context);
void DoSend(evutil_socket_t fd, short events, void* context);
void DoRecv(evutil_socket_t fd, short events, void* context);

// one event base for single thread
struct event_base* g_event_base;

//
// trans-state functions
//

void StateInitToConn(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // create new event
  struct event* new_event =
      event_new(g_event_base, fd, EV_WRITE, DoConn, state);
  if (!new_event) {
    StateToFail(fd, state, Request_Event_New_Err);
    return;
  }

  // set up new state
  TransformStateEvent(state, new_event, DontFree);
  TransformStateBuffer(state, NULL, DontFree);

  // start new state
  struct timeval tv = {CONN_TIMEOUT_SEC, 0};
  event_add(state->event, &tv);
}

void StateConnToSend(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // create new event
  struct event* new_event =
      event_new(g_event_base, fd, EV_WRITE, DoSend, state);
  if (!new_event) {
    StateToFail(fd, state, Request_Event_New_Err);
    return;
  }

  // create new buffer
  char* new_buffer = ConstructSendBuffer(state->url);
  if (!new_buffer) {
    StateToFail(fd, state, Request_Out_Of_Mem);
    return;
  }

  // set up new state
  TransformStateEvent(state, new_event, RequireFree);
  TransformStateBuffer(state, new_buffer, DontFree);
  state->n_sent = 0;

  // start new state
  event_add(state->event, NULL);
}

void StateSendToRecv(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // create new event
  struct event* new_event = event_new(g_event_base, fd, EV_READ, DoRecv, state);
  if (!new_event) {
    StateToFail(fd, state, Request_Event_New_Err);
    return;
  }

  // set up new state
  TransformStateEvent(state, new_event, RequireFree);
  TransformStateBuffer(state, NULL, RequireFree);
  state->content_length = 0;

  // start new state
  event_add(state->event, NULL);
}

void StateRecvToSucc(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // free event
  TransformStateEvent(state, NULL, RequireFree);

  // shutdown and close socket
  shutdown(fd, SHUT_RDWR);
  EVUTIL_CLOSESOCKET(fd);

  // callback on terminal state
  const char* html = strstr(state->buffer, CONTENT_START);
  if (html) {
    html += sizeof CONTENT_START - 1;
  }
  // TODO: handle error code
  state->callback(state->url, Request_Succ, html, state->context);

  // free buffer
  TransformStateBuffer(state, NULL, RequireFree);

  // clear state
  FreeState(state);
}

void StateToFail(evutil_socket_t fd,
                 RequestState* state,
                 RequestStatus status) {
  assert(state);

  // free buffer/event
  TransformStateEvent(state, NULL, RequireFree);
  TransformStateBuffer(state, NULL, RequireFree);

  // close socket
  EVUTIL_CLOSESOCKET(fd);

  // callback on terminal state
  state->callback(state->url, status, NULL, state->context);

  // clear state
  FreeState(state);
}

//
// in-state functions
//

void DoInit(evutil_socket_t fd, short events, void* context) {
  assert(!events);
  assert(context);
  RequestState* state = (RequestState*)context;

  // try connect to server
  struct sockaddr_in sa = ConstructSockAddr(state->url);
  if (connect(fd, (struct sockaddr*)&sa, sizeof sa) < 0) {
    if (EVUTIL_SOCKET_ERROR() != EINPROGRESS &&
        EVUTIL_SOCKET_ERROR() != EINTR) {
      // Init -> Fail
      StateToFail(fd, state, Request_Conn_Err);
      return;
    }
  }

  // Init -> Conn
  StateInitToConn(fd, state);
}

void DoConn(evutil_socket_t fd, short events, void* context) {
  assert(context);
  RequestState* state = (RequestState*)context;

  if (events & EV_TIMEOUT) {
    // Conn -> Fail
    StateToFail(fd, state, Request_Conn_Timeout);
    return;
  }

  assert(events & EV_WRITE);

  // check sockopt
  int err;
  socklen_t len = sizeof(err);
  assert(0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len));
  if (err) {
    // Conn -> Fail
    StateToFail(fd, state, Request_Bad_Sock_Opt);
    return;
  }

  // Conn -> Send
  StateConnToSend(fd, state);
}

void DoSend(evutil_socket_t fd, short events, void* context) {
  assert(events & EV_WRITE);
  assert(context);
  RequestState* state = (RequestState*)context;

  size_t send_upto = strlen(state->buffer);
  while (state->n_sent < send_upto) {
    ssize_t result =
        send(fd, state->buffer + state->n_sent, send_upto - state->n_sent, 0);
    if (result < 0) {
      // continue in next term
      if (EVUTIL_SOCKET_ERROR() == EAGAIN) {
        event_add(state->event, NULL);
        return;
      }

      // Send -> Fail
      StateToFail(fd, state, Request_Send_Err);
      return;
    }
    assert(result != 0);

    // continue sending
    state->n_sent += (size_t)result;
  }

  // finish sending
  assert(state->n_sent == send_upto);

  // Send -> Recv
  StateSendToRecv(fd, state);
}

void DoRecv(evutil_socket_t fd, short events, void* context) {
  assert(events & EV_READ);
  assert(context);
  RequestState* state = (RequestState*)context;

  char buffer[RECV_BUFFER_SIZE];
  while (1) {
    ssize_t result = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (result < 0) {
      // continue in next term
      if (EVUTIL_SOCKET_ERROR() == EAGAIN) {
        event_add(state->event, NULL);
        return;
      }

      // Recv -> Fail
      StateToFail(fd, state, Request_Recv_Err);
      return;

    } else if (result == 0) {
      // succeeded
      break;
    }

    // continue recving
    {
      // allocate/reallocate memory of |recv_buffer|
      size_t previous_len = 0;
      if (!state->buffer) {
        // init |recv_buffer| with malloc
        state->buffer = (char*)malloc((size_t)result + 1);
      } else {
        // update |previous_len|
        previous_len = strlen(state->buffer);

        // realloc memory of |recv_buffer|
        state->buffer =
            (char*)realloc(state->buffer, previous_len + (size_t)result + 1);
      }

      if (!state->buffer) {
        // Recv -> Fail
        StateToFail(fd, state, Request_Out_Of_Mem);
        return;
      }

      // make |recv_buffer| C-style string
      state->buffer[previous_len] = 0;

      // append |buffer| to |recv_buffer|
      strncat(state->buffer, buffer, (size_t)result);
    }

    // process content length
    {
      // try parse 'Content-Length:' from |recv_buffer|
      if (!state->content_length) {
        const char* cont_len_str = strstr(state->buffer, CONTENT_LENGTH_START);
        if (cont_len_str) {
          size_t val = 0;
          if (EOF != sscanf(cont_len_str, CONTENT_LENGTH_TEMPLATE, &val))
            state->content_length = val;
        }
      }

      // check length of content
      if (state->content_length) {
        const char* cont_str = strstr(state->buffer, CONTENT_START);
        if (cont_str) {
          // move to the start of content data
          cont_str += sizeof CONTENT_START - 1;

          // check if recv sufficient data
          if (cont_str && strlen(cont_str) >= state->content_length) {
            assert(strlen(cont_str) == state->content_length);

            // succeeded
            break;
          }
        }
      }
    }
  }

  // Recv -> Succ
  StateRecvToSucc(fd, state);
}

//
// export functions
//

void Request(const char* url, request_callback_fn callback, void* context) {
  assert(url);

  // init |g_event_base| only once
  if (!g_event_base) {
    g_event_base = event_base_new();
    assert(g_event_base);
  }

  // create socket or add to pending list
  evutil_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd <= 0) {
    if (EVUTIL_SOCKET_ERROR() == EMFILE || EVUTIL_SOCKET_ERROR() == ENFILE) {
      // reach fd limit
      callback(url, Request_Fd_Limit, NULL, context);
    } else {
      // unexpected socket error
      callback(url, Request_Socket_Err, NULL, context);
    }
    return;
  }

  // make socket non-blocking
  assert(0 == evutil_make_socket_nonblocking(fd));

  // init state for current request
  RequestState* state = CreateState(url, callback, context);
  if (!state) {
    EVUTIL_CLOSESOCKET(fd);
    callback(url, Request_Out_Of_Mem, NULL, context);
    return;
  }

  // start state machine
  DoInit(fd, 0, state);
}

void DispatchLibEvent() {
  if (!g_event_base)
    return;

  // start event loop
  event_base_dispatch(g_event_base);

  // free event base
  event_base_free(g_event_base);

  // check leaks
  assert(g_request_state_count == 0);
}
