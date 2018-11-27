
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

void RequestCallback(const char* res, const char* html, void* context);

void ProcessUrl(const char* url, void* context) {
  assert(url);
  const char* src_url = (const char*)context;

  if (src_url) {
    // TODO: use graph to store links between urls
    printf("%s -> %s\n", src_url, url);
  }

  // ignore |url| in url set
  if (BloomFilterTest(url))
    return;

  // add |url| to url set
  BloomFilterAdd(url);

  // use |url| as start node to crawl pages
  // async call |RequestCallback|
  Request(url, RequestCallback, CopyString(url));
}

void RequestCallback(const char* res, const char* html, void* context) {
  (void)(res);
  char* src_url = (char*)context;
  if (!src_url) {
    fprintf(stderr, "failed to fetch unknown_url\n");
    return;
  }

  if (!html) {
    fprintf(stderr, "failed to fetch %s\n", src_url);
    free((void*)src_url);
    return;
  }

  // sync call |ProcessUrl|
  ParseAtagUrls(html, ProcessUrl, src_url);
  free((void*)src_url);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: ./crawler URL\n");
    return 1;
  }

  InitLibEvent();

  // use |argv[1]| to start crawl tasks
  ProcessUrl(argv[1], NULL);

  DispatchLibEvent();

  return 0;
}
