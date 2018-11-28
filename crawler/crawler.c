
// Simple web crawler
//   by BOT Man & ZhangHan, 2018

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bloom_filter.h"
#include "html_parser.h"
#include "http_client.h"
#include "string_helper.h"
#include "url_map.h"

#define HANDLED_URL_SET_SIZE (160000 * 100)
#define PAGE_URL_SET_SIZE (1000 * 100)

void RequestCallback(const char* res, const char* html, void* context);

typedef struct {
  // referred source url
  const char* src_url;

  // local url-set for current page to avoid dup |ConnectUrls|
  BloomFilter* page_url_set;
} ProcessUrlContext;

// global url-set for crawling pages to avoid dup |Request|
BloomFilter* g_handled_url_set;

void ProcessUrl(const char* url, void* context) {
  assert(url);
  const ProcessUrlContext* page_context = (const ProcessUrlContext*)context;

  // handle page connections
  if (page_context) {
    assert(page_context->src_url);
    assert(page_context->page_url_set);

    // ensure |src_url| in |g_handled_url_set| already
    assert(BloomFilterTest(g_handled_url_set, page_context->src_url));

    // check |url| in |page_url_set|
    if (!BloomFilterTest(page_context->page_url_set, url)) {
      // add |url| to |page_url_set|
      BloomFilterAdd(page_context->page_url_set, url);

      // connect |src_url| to |url|
      ConnectUrls(page_context->src_url, url);
    }
  }

  // handle crawl tasks
  // check |url| in |g_handled_url_set|
  if (!BloomFilterTest(g_handled_url_set, url)) {
    // add |url| to |g_handled_url_set|
    BloomFilterAdd(g_handled_url_set, url);

    // use |url| as start node to crawl pages
    // async once call |RequestCallback|
    Request(url, RequestCallback, CopyString(url));
  }
}

void RequestCallback(const char* res, const char* html, void* context) {
  (void)(res);
  char* url = (char*)context;
  if (!url) {
    fprintf(stderr, "failed to fetch unknown_url\n");
    return;
  }

  if (!html) {
    fprintf(stderr, "failed to fetch %s\n", url);
    free((void*)url);
    return;
  }

  ProcessUrlContext page_context = {
      url,
      CreateBloomFilter(PAGE_URL_SET_SIZE),
  };

  // sync multi call |ProcessUrl|
  ParseAtagUrls(html, ProcessUrl, &page_context);

  free((void*)page_context.src_url);
  FreeBloomFilter(page_context.page_url_set);
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

  g_handled_url_set = CreateBloomFilter(HANDLED_URL_SET_SIZE);
  assert(g_handled_url_set);

  // use |argv[1]| to start crawl tasks
  ProcessUrl(argv[1], NULL);

  // dispatch crawl tasks
  DispatchLibEvent();

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
  return 0;
}
