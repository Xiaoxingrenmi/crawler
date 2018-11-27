
// Simple web crawler
//   by BOT Man & ZhangHan, 2018

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bloom_filter.h"
#include "html_parser.h"
#include "http_client.h"
#include "link_list.h"
#include "string_helper.h"

#define DEBUG

void AddUrlMap(const char* url, void* context) {
  if (!url || !context)
    return;
  ListNode* task_queue = (ListNode*)context;

  if (BloomFilterTest(url))
    return;

#ifdef DEBUG
  printf("> %s\n", url);
#endif  // DEBUG

  BloomFilterAdd(url);
  LinkListPushBack(task_queue, CopyString(url));
}

void ParseUrlsInHtml(const char* res, const char* html, void* context) {
  (void)(res);
  if (!html || !context)
    return;

#ifdef DEBUG
  printf("\nparsed urls:\n");
#endif  // DEBUG

  ParseAtagUrls(html, AddUrlMap, context);
}

void StartCrawlTask(const char* start_url) {
  if (!start_url)
    return;

  ListNode* task_queue = CreateListNode(NULL);
  if (!task_queue)
    return;

  BloomFilterAdd(start_url);
  LinkListPushBack(task_queue, CopyString(start_url));

  while (1) {
    char* url = LinkListPop(task_queue);
    if (!url)
      break;

#ifdef DEBUG
    printf("\n------------------------\nrequested url:\n%s\n", url);
#endif  // DEBUG

    Request(url, ParseUrlsInHtml, task_queue);

    free((void*)url);
  }

  DeleteListNode(task_queue);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: ./crawler URL\n");
    return 1;
  }

  // use |start_url| to start crawl tasks
  const char* start_url = argv[1];
  StartCrawlTask(start_url);

  // check memory leaks
  AssertNoListNode();
  return 0;
}
