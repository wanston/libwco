//
// Created by tong on 19-4-4.
//
#include <stdio.h>
#include <malloc.h>
#include "wco_tools.h"

int main(){
    // 测试WcoQueue
    WcoQueue *q = WcoQueueCreate();
    WcoQueuePush(q, 0);
    WcoQueuePush(q, 1);
    WcoQueuePush(q, 2);
    printf("%ld\n", (long)WcoQueuePop(q));
    printf("%ld\n", (long)WcoQueuePop(q));
    WcoQueuePush(q, 3);
    WcoQueuePush(q, 4);
    while(!WcoQueueEmpty(q)){
        printf("%ld\n", (long)WcoQueuePop(q));
    }
    WcoQueueDestroy(q);
    printf("\n");

    // 测试
    WcoHeapNode *n;
    n = malloc(sizeof(WcoHeapNode));

    WcoBigRootHeap *h = WcoHeapCreate();

    n = malloc(sizeof(WcoHeapNode));
    n->time = 4;
    WcoHeapPush(h, n);
    n = malloc(sizeof(WcoHeapNode));
    n->time = 3;
    WcoHeapPush(h, n);
    n = malloc(sizeof(WcoHeapNode));
    n->time = 9;
    WcoHeapPush(h, n);
    n = malloc(sizeof(WcoHeapNode));
    n->time = 6;
    WcoHeapPush(h, n);

    printf("%ld\n", WcoHeapTop(h)->time);
    WcoHeapPop(h);
    printf("%ld\n", WcoHeapTop(h)->time);
    WcoHeapPop(h);
    printf("%ld\n", WcoHeapTop(h)->time);
    WcoHeapPop(h);

    printf("%ld\n", WcoHeapTop(h)->time);

    n = malloc(sizeof(WcoHeapNode));
    n->time = 3;
    WcoHeapPush(h, n);
    n = malloc(sizeof(WcoHeapNode));
    n->time = 8;
    WcoHeapPush(h, n);
    n = malloc(sizeof(WcoHeapNode));
    n->time = 7;
    WcoHeapPush(h, n);

    while(!WcoHeapEmpty(h)){
        printf("%ld\n", WcoHeapTop(h)->time);
        WcoHeapPop(h);
    }

    for(int i=0; i<100; i++){
        n = malloc(sizeof(WcoHeapNode));
        n->time = i;
        WcoHeapPush(h, n);
    }

    while(!WcoHeapEmpty(h)){
        printf("%ld\n", WcoHeapTop(h)->time);
        WcoHeapPop(h);
    }

    WcoHeapDestroy(h);
    return 0;
}
