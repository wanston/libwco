//
// Created by tong on 19-4-2.
//
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h>
#include "wco_tools.h"

struct WcoLinkNode_t {
    void* data;
    struct WcoLinkNode_t* next;
};

typedef struct WcoLinkNode_t WcoLinkNode;

struct WcoQueue_t{
    WcoLinkNode* head; // 哨兵，非实际节点
    WcoLinkNode* tail;
};


WcoQueue* WcoQueueCreate(){
    WcoQueue* q = (WcoQueue*)malloc(sizeof(WcoQueue));
    q->head = (WcoLinkNode*)calloc(1, sizeof(WcoLinkNode));
    q->tail = q->head;
    return q;
}


void WcoQueuePush(WcoQueue *q, void *data){
    WcoLinkNode* node = (WcoLinkNode*)malloc(sizeof(WcoLinkNode));
    node->data = data;
    node->next = NULL;
    q->tail->next = node;
    q->tail = node;
}


void* WcoQueuePop(WcoQueue *q){
    assert(!WcoQueueEmpty(q));
    WcoLinkNode* node = q->head->next;
    void* data = node->data;
    q->head->next = node->next;
    free(node);
    return data;
}


bool WcoQueueEmpty(WcoQueue *q){
    return q->head == q->tail;
}


void WcoQueueDestroy(WcoQueue* q){
    WcoLinkNode *l=q->head, *pre;
    while((pre=l)){
        l = l->next;
        free(pre);
    }
    free(q);
}


struct WcoBigRootHeap_t{
    WcoHeapNode *content;
    size_t capacity; // 元素的容量，content的空间是capacity+1
    size_t size; // 元素的数目
};


WcoBigRootHeap* WcoHeapCreate(){
    WcoBigRootHeap *heap = (WcoBigRootHeap*)calloc(1, sizeof(WcoBigRootHeap));
    heap->capacity = 32;
    heap->content = (WcoHeapNode*)malloc(sizeof(WcoHeapNode)*(heap->capacity+1));
    return heap;
}


void WcoHeapPush(WcoBigRootHeap* heap, WcoHeapNode node){
    if(heap->size == heap->capacity){
        size_t newCap = heap->capacity*2;
        WcoHeapNode *l = (WcoHeapNode*)malloc(sizeof(WcoHeapNode)*(newCap+1));
        memcpy(l, heap->content, sizeof(WcoHeapNode)*(heap->capacity+1));
        free(heap->content);
        heap->content = l;
        heap->capacity = newCap;
    }

    size_t hole = heap->size+1;
    for(; hole > 1 && node.time > heap->content[hole/2].time; hole /= 2){
        heap->content[hole] = heap->content[hole/2];
    }
    heap->content[hole] = node;
    heap->size++;
}

WcoHeapNode WcoHeapTop(WcoBigRootHeap* heap){
    assert(!WcoHeapEmpty(heap));
    return heap->content[1];
}

void WcoHeapPop(WcoBigRootHeap* heap){
    assert(!WcoHeapEmpty(heap));
    size_t hole = 1;
    WcoHeapNode node = heap->content[heap->size];
    while(hole*2 <= heap->size){
        size_t max=hole*2;
        if(hole*2+1 <= heap->size && heap->content[hole*2+1].time > heap->content[hole*2].time){
            max = hole*2+1;
        }
        if(node.time < heap->content[max].time){
            heap->content[hole] = heap->content[max];
        }else{
            break;
        }
        hole = max;
    }
    heap->size--;
}


bool WcoHeapEmpty(WcoBigRootHeap* heap){
    return heap->size == 0;
}


void WcoHeapDestroy(WcoBigRootHeap* heap){
    free(heap->content);
    free(heap);
}


long WcoGetCurrentMsTime(){
    struct timespec tp = {0};
    int res = clock_gettime(CLOCK_REALTIME, &tp);
    assert(res >= 0);
    return tp.tv_sec*1000 + tp.tv_nsec / 1000000;
}