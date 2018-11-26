
// Common Link List Implementation
//   by BOT Man & ZhangHan, 2018

#ifndef LINK_LIST
#define LINK_LIST

// link list node is url in this function
typedef char* ListNodeValue;

typedef struct _ListNode {
  ListNodeValue value;
  struct _ListNode* next;
} ListNode;

ListNode* CreateListNode(ListNodeValue value);
void DeleteListNode(ListNode* node);

ListNode* LinkListPushBack(ListNode* head, ListNodeValue value);
ListNodeValue LinkListPop(ListNode* head);

// check memory leaks
void AssertNoListNode();

#endif  // LINK_LIST
