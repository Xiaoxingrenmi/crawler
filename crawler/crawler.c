
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

void RequestCallback(const char* res, const char* html, void* context);

void ProcessUrl(const char* url, void* context) {
  assert(url);
  const char* src_url = (const char*)context;

  if (src_url) {
    printf("> %s -> %s\n", src_url, url);

    // ensure |src_url| in map already
    assert(BloomFilterTest(src_url));

    // connect |src_url| to |url|
    ConnectUrls(src_url, url);
  }

  // check |url| in map
  if (!BloomFilterTest(url)) {
    // add |url| to map
    BloomFilterAdd(url);

    // use |url| as start node to crawl pages
    // async call |RequestCallback|
    Request(url, RequestCallback, CopyString(url));
  }
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
  return 0;
}
