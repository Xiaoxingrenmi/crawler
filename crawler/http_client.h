
// Naive http client
//   by BOT Man & ZhangHan, 2018

#ifndef HTTP_CLIENT
#define HTTP_CLIENT

typedef void (*request_callback_fn)(const char* res,
                                    const char* html,
                                    void* context);

void Request(const char* url, request_callback_fn callback, void* context);

void DispatchLibEvent();

#endif  // HTTP_CLIENT
