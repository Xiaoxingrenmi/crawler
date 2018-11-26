
// Common Link List Implementation
//   by BOT Man & ZhangHan, 2018

#include "link_list.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static size_t g_list_node_count;

ListNode* CreateListNode(ListNodeValue value) {
  ListNode* ret = (ListNode*)malloc(sizeof(ListNode));
  if (!ret)
    return NULL;

  ++g_list_node_count;
  memset(ret, 0, sizeof(ListNode));
  ret->value = value;
  return ret;
}

void DeleteListNode(ListNode* node) {
  if (!node)
    return;

  free((void*)node);
  --g_list_node_count;
}

ListNode* LinkListPushBack(ListNode* head, ListNodeValue value) {
  if (!head)
    return NULL;

  ListNode* last = CreateListNode(value);
  if (!last)
    return NULL;

  ListNode* tail = head;
  for (; tail->next != NULL; tail = tail->next)
    ;
  tail->next = last;

  return last;
}

ListNodeValue LinkListPop(ListNode* head) {
  if (!head)
    return NULL;

  ListNode* first = head->next;
  if (!first)
    return NULL;

  ListNodeValue value = first->value;

  head->next = first->next;
  DeleteListNode(first);

  return value;
}

void AssertNoListNode() {
  assert(0 == g_list_node_count);
}
