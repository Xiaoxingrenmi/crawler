
// Simple web crawler
//   by BOT Man & ZhangHan, 2018

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "bloom_filter.h"
#include "html_parser.h"
#include "http_client.h"
#include "string_helper.h"
#include "url_helper.h"
#include "url_map.h"

#define HANDLED_URL_SET_SIZE (160000 * 100)
#define PAGE_URL_SET_SIZE (1000 * 100)

void RequestCallback(const char* url,
                     RequestStatus status,
                     const char* html,
                     void* context);

struct PendingRequest {
  const char* url;
  TAILQ_ENTRY(PendingRequest) _entries;
};

TAILQ_HEAD(, PendingRequest) g_pending_request_queue;

typedef struct {
  // referred source url
  const char* src_url;

  // local url-set for current page to avoid dup |ConnectUrls|
  BloomFilter* page_url_set;
} ProcessUrlContext;

// global url-set for crawling pages to avoid dup |Request|
BloomFilter* g_handled_url_set;

void ProcessUrl(const char* raw_url, void* context) {
  assert(raw_url);
  const ProcessUrlContext* page_context = (const ProcessUrlContext*)context;

  // fix |raw_url| to absolute and uniform |url|
  char* url = page_context ? FixUrl(raw_url, page_context->src_url)
                           : FixUrl(raw_url, NULL);

  // ignore ill-formed |url|
  if (!url) {
    fprintf(stderr, "failed to parse %s\n", raw_url);
    return;
  }

  // handle page connections (test page_url_set, handle by ConnectUrls)
  if (page_context) {
    assert(page_context->src_url);
    assert(page_context->page_url_set);

    // ensure |src_url| in |g_handled_url_set| already
    assert(BloomFilterTest(g_handled_url_set, page_context->src_url));

    if (!BloomFilterTest(page_context->page_url_set, url)) {
      BloomFilterAdd(page_context->page_url_set, url);

      // connect |src_url| to |url|
      ConnectUrls(page_context->src_url, url);
    }
  }

  // handle crawl tasks (test g_handled_url_set, handle by Request)
  if (!BloomFilterTest(g_handled_url_set, url)) {
    BloomFilterAdd(g_handled_url_set, url);

    // use |url| as start node to crawl pages
    // async once call |RequestCallback|
    Request(url, RequestCallback, NULL);
  }

  free((void*)url);
}

void RequestCallback(const char* url,
                     RequestStatus status,
                     const char* html,
                     void* context) {
  assert(url);
  (void)(context);

  if (status == Request_Fd_Limit) {
    // push back current request to pending list
    struct PendingRequest* pending_request =
        (struct PendingRequest*)malloc(sizeof(struct PendingRequest));
    pending_request->url = url;

    TAILQ_INSERT_TAIL(&g_pending_request_queue, pending_request, _entries);
    return;
  }

  // try start pending request(s)
  while (!TAILQ_EMPTY(&g_pending_request_queue)) {
    struct PendingRequest* request = TAILQ_FIRST(&g_pending_request_queue);
    TAILQ_REMOVE(&g_pending_request_queue, request, _entries);

    // may re-enter RequestCallback
    Request(request->url, RequestCallback, NULL);
    free((void*)request);
  }

  if (status != Request_Succ || !html) {
    fprintf(stderr, "failed to fetch %s (%d)\n", url, status);
    return;
  }

  BloomFilter* page_url_set = CreateBloomFilter(PAGE_URL_SET_SIZE);
  ProcessUrlContext page_context = {url, page_url_set};

  // sync multi call |ProcessUrl|
  ParseAtagUrls(html, ProcessUrl, &page_context);

  FreeBloomFilter(page_url_set);
}

void YieldUrlConnectionIndexCallback(const char* url,
                                     size_t index,
                                     void* context) {
  assert(url);
  assert(index);
  assert(context);
  FILE* output_file = (FILE*)context;

  fprintf(output_file, "- %3lu %s\n", index, url);
}

void YieldUrlConnectionPairCallback(size_t src, size_t dst, void* context) {
  assert(src);
  assert(dst);
  assert(context);
  FILE* output_file = (FILE*)context;

  fprintf(output_file, "+ %3lu %3lu\n", src, dst);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: ./crawler URL [OUTPUT_FILE]\n");
    return 1;
  }

  TAILQ_INIT(&g_pending_request_queue);

  g_handled_url_set = CreateBloomFilter(HANDLED_URL_SET_SIZE);
  assert(g_handled_url_set);

  // use |argv[1]| to start crawl tasks
  ProcessUrl(argv[1], NULL);

  // dispatch crawl tasks
  DispatchLibEvent();

  // use output_file if exists
  FILE* output_file = NULL;
  if (argc >= 3)
    output_file = fopen(argv[2], "w");

  // output results
  YieldUrlConnectionIndex(YieldUrlConnectionIndexCallback,
                          output_file ? output_file : stdout);
  YieldUrlConnectionPair(YieldUrlConnectionPairCallback,
                         output_file ? output_file : stdout);

  if (output_file)
    fclose(output_file);

  FreeBloomFilter(g_handled_url_set);

  AssertBloomFilterNoLeak();
  assert(TAILQ_EMPTY(&g_pending_request_queue));
  return 0;
}
