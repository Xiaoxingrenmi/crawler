
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
// For queue
#include <sys/queue.h>

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

char* ConstructSendBuffer(const char* url) {
  char* ret = NULL;
  char* host = ParseHost(url);
  char* request = ParseRequest(url);

  if (host && request) {
    char buffer[SEND_BUFFER_SIZE];
    sprintf(buffer, HTTP_GET_TEMPLATE, request, host);
    ret = CopyString(buffer);
  }

  if (host)
    free((void*)host);
  if (request)
    free((void*)request);
  return ret;
}

struct sockaddr_in ConstructSockAddr(const char* url) {
  struct sockaddr_in sa = {0};

  char* host = ParseHost(url);
  uint16_t port = ParsePort(url);

  if (host && port) {
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
  }

  if (host)
    free((void*)host);
  return sa;
}

typedef struct {
  // requested url
  char* url;

  // callback data
  request_callback_fn callback;
  void* context;

  // event of current state
  struct event* event;

  // buffer of current state
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

// one event base for single thread
struct event_base* g_event_base;

RequestState* CreateState(const char* url,
                          request_callback_fn callback,
                          void* context) {
  RequestState* ret = (RequestState*)malloc(sizeof(RequestState));
  assert(ret);
  if (!ret)
    return NULL;
  memset(ret, 0, sizeof(RequestState));

  ++g_request_state_count;

  ret->url = CopyString(url);
  ret->callback = callback;
  ret->context = context;
  return ret;
}

void FreeState(RequestState* state) {
  assert(state);

  free((void*)state->url);
  free((void*)state);

  --g_request_state_count;
}

void DoInit(evutil_socket_t fd, short events, void* context);
void DoConn(evutil_socket_t fd, short events, void* context);
void DoSend(evutil_socket_t fd, short events, void* context);
void DoRecv(evutil_socket_t fd, short events, void* context);

// Succ|Fail -> Clear
void ToClearState(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // close socket
  shutdown(fd, SHUT_RDWR);
  EVUTIL_CLOSESOCKET(fd);

  // clear state
  FreeState(state);
}

// * -> Fail
void ToFailState(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // callback on terminal state
  state->callback(NULL, NULL, state->context);

  // clear from-state
  if (state->event)
    event_free(state->event);
  if (state->buffer)
    free((void*)state->buffer);

  // Fail -> Clear
  ToClearState(fd, state);
}

// Init -> Conn
void ToConnState(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // clear from-state
  assert(!state->event);
  assert(!state->buffer);

  // init to-state
  state->event = event_new(g_event_base, fd, EV_WRITE, DoConn, state);
  state->buffer = NULL;

  // handle fatal error
  assert(state->event);
  if (!state->event) {
    ToFailState(fd, state);
    return;
  }

  // start new state
  struct timeval tv = {CONN_TIMEOUT_SEC, 0};
  event_add(state->event, &tv);
}

// Conn -> Send
void ToSendState(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // clear from-state
  assert(state->event);
  assert(!state->buffer);

  event_free(state->event);

  // init to-state
  state->event = event_new(g_event_base, fd, EV_WRITE, DoSend, state);
  state->buffer = ConstructSendBuffer(state->url);
  state->n_sent = 0;

  // handle fatal error
  assert(state->event);
  assert(state->buffer);
  if (!state->event || !state->buffer) {
    ToFailState(fd, state);
    return;
  }

  // start to-state
  event_add(state->event, NULL);
}

// Send -> Recv
void ToRecvState(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // clear from-state
  assert(state->event);
  assert(state->buffer);

  event_free(state->event);
  free((void*)state->buffer);

  // init to-state
  state->event = event_new(g_event_base, fd, EV_READ, DoRecv, state);
  state->buffer = NULL;  // allocated in DoRecv
  state->content_length = 0;

  // handle fatal error
  assert(state->event);
  if (!state->event) {
    ToFailState(fd, state);
    return;
  }

  // start to-state
  event_add(state->event, NULL);
}

// Recv -> Succ
void ToSuccState(evutil_socket_t fd, RequestState* state) {
  assert(state);

  // callback on terminal state
  const char* html = strstr(state->buffer, CONTENT_START);
  if (html) {
    html += sizeof CONTENT_START - 1;
  }
  state->callback(state->buffer, html, state->context);

  // clear from-state
  assert(state->event);
  assert(state->buffer);

  event_free(state->event);
  free((void*)state->buffer);

  // Succ -> Clear
  ToClearState(fd, state);
}

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
      ToFailState(fd, state);
      return;
    }
  }

  // Init -> Conn
  ToConnState(fd, state);
}

void DoConn(evutil_socket_t fd, short events, void* context) {
  assert(context);
  RequestState* state = (RequestState*)context;
  if (events & EV_WRITE) {
    int err;
    socklen_t len = sizeof(err);
    assert(0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len));
    if (err) {
      // invalid sockopt

      // Conn -> Fail
      ToFailState(fd, state);
      return;
    }
  } else {
    // timeout
    assert(events & EV_TIMEOUT);

    // Conn -> Fail
    ToFailState(fd, state);
    return;
  }

  // Conn -> Send
  ToSendState(fd, state);
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
      ToFailState(fd, state);
      return;
    }
    assert(result != 0);

    // continue sending
    state->n_sent += (size_t)result;
  }

  // finish sending
  assert(state->n_sent == send_upto);

  // Send -> Recv
  ToRecvState(fd, state);
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
      ToFailState(fd, state);
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
      assert(state->buffer);

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
  ToSuccState(fd, state);
}

// pending request if reaching fd limit
struct PendingRequest {
  const char* url;
  request_callback_fn callback;
  void* context;

  TAILQ_ENTRY(PendingRequest) _entries;
};

TAILQ_HEAD(, PendingRequest) g_pending_request_queue;

void RequestImpl(const char* url, request_callback_fn callback, void* context) {
  // create socket or add to pending list
  evutil_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    // reach fd limit
    if (EVUTIL_SOCKET_ERROR() == EMFILE || EVUTIL_SOCKET_ERROR() == ENFILE) {
      // push back current request to pending list
      struct PendingRequest* pending_request =
          (struct PendingRequest*)malloc(sizeof(struct PendingRequest));
      pending_request->url = url;
      pending_request->callback = callback;
      pending_request->context = context;

      TAILQ_INSERT_TAIL(&g_pending_request_queue, pending_request, _entries);
      return;
    }

    // unexpected socket error
    callback(NULL, NULL, context);
    return;
  }

  // make socket non-blocking
  assert(0 == evutil_make_socket_nonblocking(fd));

  // init state for current request
  RequestState* state = CreateState(url, callback, context);
  if (!state) {
    EVUTIL_CLOSESOCKET(fd);
    callback(NULL, NULL, context);
    return;
  }

  // start by Init state
  DoInit(fd, 0, state);
}

void Request(const char* url, request_callback_fn callback, void* context) {
  assert(url);

  // init |g_event_base| and |g_pending_request_queue| only once
  if (!g_event_base) {
    TAILQ_INIT(&g_pending_request_queue);

    g_event_base = event_base_new();
    assert(g_event_base);
  }

  // process pending request(s)
  while (!TAILQ_EMPTY(&g_pending_request_queue)) {
    struct PendingRequest* request = TAILQ_FIRST(&g_pending_request_queue);
    TAILQ_REMOVE(&g_pending_request_queue, request, _entries);

    RequestImpl(request->url, request->callback, request->context);
    free((void*)request);
  }

  // process current request
  RequestImpl(url, callback, context);
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
  assert(TAILQ_EMPTY(&g_pending_request_queue));
}
