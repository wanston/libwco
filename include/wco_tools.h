//
// Created by tong on 19-4-2.
//

#ifndef PROJECT_WCO_TOOLS_H
#define PROJECT_WCO_TOOLS_H

#include <stdbool.h>

struct WcoQueue_t;
typedef struct WcoQueue_t WcoQueue;

WcoQueue* WcoQueueCreate();
void WcoQueuePush(WcoQueue *q, void *data);
void* WcoQueuePop(WcoQueue *q);
bool WcoQueueEmpty(WcoQueue *q);
void WcoQueueDestroy(WcoQueue* q);

typedef struct {
    long time;
    int fd;
    bool valid;
} WcoHeapNode;

struct WcoBigRootHeap_t;
typedef struct WcoBigRootHeap_t WcoBigRootHeap;

WcoBigRootHeap* WcoHeapCreate();
void WcoHeapPush(WcoBigRootHeap* heap, WcoHeapNode *node);
WcoHeapNode* WcoHeapTop(WcoBigRootHeap* heap);
void WcoHeapPop(WcoBigRootHeap* heap);
bool WcoHeapEmpty(WcoBigRootHeap* heap);
void WcoHeapDestroy(WcoBigRootHeap* heap);


long WcoGetCurrentMsTime();
#endif //PROJECT_WCO_TOOLS_H
