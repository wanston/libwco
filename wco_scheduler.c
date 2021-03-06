//
// Created by tong on 19-4-2.
//
#ifdef DEBUG
#include <stdio.h>
#endif
#include <sys/epoll.h>
#include <assert.h>
#include <memory.h>
#include "wco_scheduler.h"

/*
 * TODO: 1. 优化WcoScheduler中，fd和协程的存储方式
 * TODO: 2. 允许两个协程同时对同一fd监听不同的事件
 * TODO: 3. WcoAddCoToScheduler接口是否要添加timeout参数？ 最好是设计接口控制协程的fd的超时时间
 * */

static __thread WcoScheduler *wcoScheduler;

int WcoAddEventToScheduler(WcoScheduler* , WcoRoutine*co, int fd, uint32_t events, struct timeval time_out);
void WcoRemoveEventFromScheduler(WcoScheduler* , WcoRoutine*co, int fd, uint32_t events);


WcoScheduler *WcoGetScheduler(){
    if(!wcoScheduler){
        wcoScheduler = (WcoScheduler*)malloc(sizeof(WcoScheduler));
        memset(wcoScheduler, 0, sizeof(WcoScheduler));
        wcoScheduler->pendingCoQueue = WcoQueueCreate();
        wcoScheduler->epollFd = epoll_create(1);
        assert(wcoScheduler->epollFd >= 0);
        wcoScheduler->epollElemsSize = 0;
        wcoScheduler->heap = WcoHeapCreate();
    }

    return wcoScheduler;
};


void WcoAddCoToScheduler(WcoScheduler* scheduler, WcoRoutine* co){
    WcoQueuePush(scheduler->pendingCoQueue, co);
}

/*
 *
 * */
void WcoRunScheduler(WcoScheduler *scheduler) {
    // 1. 运行pending的协程
    WcoQueue *q = scheduler->pendingCoQueue;
    while (!WcoQueueEmpty(q)) {
        WcoRoutine *co = (WcoRoutine *) WcoQueuePop(q);
        WcoResume(co);
    }
    // 2. 运行事件循环
    struct epoll_event result[MAX_SIZE];

    for (;;) {
        int res = epoll_wait(scheduler->epollFd, result, MAX_SIZE, 1);// 10ms
        assert(res >= 0);
        // 2.1 处理活跃的fd
        for (int i = 0; i < res; i++) {
            int fd = result[i].data.fd;
            assert(scheduler->activeSize < MAX_SIZE);
            if(scheduler->epollElems[fd].timer){
                scheduler->epollElems[fd].timer->valid = false;
#ifdef DEBUG
                printf("valid false co %x timer %x\n", scheduler->epollElems[fd].co, scheduler->epollElems[fd].timer);
#endif
            }
            scheduler->activeCos[scheduler->activeSize++] = scheduler->epollElems[fd].co;
#ifdef DEBUG
            printf("active epoll co %x\n", scheduler->epollElems[fd].co);
#endif
            // TODO: 把对应的timer标记为失效

        }
        // 2.2 处理pending的协程
        while (!WcoQueueEmpty(q)) {
            scheduler->activeCos[scheduler->activeSize++] = (WcoRoutine *) WcoQueuePop(q);
#ifdef DEBUG
            printf("active queue co %x\n", scheduler->activeCos[scheduler->activeSize-1]);
#endif
        }
        // 2.3 处理超时的fd
        long now = WcoGetCurrentMsTime();
        WcoHeapNode *node;
        while(!WcoHeapEmpty(scheduler->heap) && (node = WcoHeapTop(scheduler->heap))->time < now){ // 现在时间已经超过了预定时间
            if(node->valid){
#ifdef DEBUG
                printf("valid true co %x timer %x\n", scheduler->epollElems[node->fd].co, scheduler->epollElems[node->fd].timer);
#endif
                assert(scheduler->activeSize < MAX_SIZE);
                scheduler->activeCos[scheduler->activeSize++] = scheduler->epollElems[node->fd].co;
//#ifdef DEBUG
//                printf("active timeout co %x\n", scheduler->activeCos[scheduler->activeSize-1]);
//#endif
            }
            WcoHeapPop(scheduler->heap);
            free(node);
        }
        // 2.4 运行active的协程
        for(int i=0; i<scheduler->activeSize; i++){
            WcoResume(scheduler->activeCos[i]);
            // 到这，肯定是yield回来的
            // 1. 要么是执行完co yield回来的；
            // 2. 要么是co执行中，注册了co，yield回来的；
            if(scheduler->activeCos[i]->isEnd){ // 如果是第一种情况
#ifdef DEBUG
                printf("destroy co %x\n", scheduler->activeCos[i]);
#endif
                WcoDestroy(scheduler->activeCos[i]);
            }
        }
        scheduler->activeSize = 0;
        // 2.5 判断结束条件
         if(scheduler->epollElemsSize == 0 && WcoQueueEmpty(scheduler->pendingCoQueue)){ // 因为2.4中可能又新加了协程进来
#ifdef DEBUG
             printf("normal exit.\n");
#endif
             break;
         }
    }
}


/*
 * read 系统调用中要：
 * epoll_add，把fd和WcoRoutine关联，存储起来。
 * */
int WcoAddEventToScheduler(WcoScheduler* scheduler, WcoRoutine* co, int fd, uint32_t events, struct timeval time_out){
    long timeout = time_out.tv_sec*1000 + time_out.tv_usec/1000;
    assert(timeout >= 0);

    scheduler->epollElems[fd].events = events;
    scheduler->epollElems[fd].co = co;
    scheduler->epollElems[fd].fd = fd;
    scheduler->epollElems[fd].timeout = timeout;
    ++scheduler->epollElemsSize;

    struct epoll_event e = {0};
    e.events = events;
    e.data.fd = fd;
    assert(epoll_ctl(scheduler->epollFd, EPOLL_CTL_ADD, fd, &e) == 0);

    if(timeout > 0){ // timeout为0表示不设置超时
        WcoHeapNode *node = (WcoHeapNode*)malloc(sizeof(WcoHeapNode));
        node->time = WcoGetCurrentMsTime() + timeout;
        node->fd = fd;
        node->valid = true;
        scheduler->epollElems[fd].timer = node;
        WcoHeapPush(scheduler->heap, node);
    }

    return 0;
}


void WcoRemoveEventFromScheduler(WcoScheduler* scheduler, WcoRoutine*co, int fd, uint32_t events){
    struct epoll_event e = {0};
    e.events = events;
    assert(epoll_ctl(scheduler->epollFd, EPOLL_CTL_DEL, fd, &e) == 0);
    scheduler->epollElems[fd].co = NULL;
    if(scheduler->epollElems[fd].timer) { // timer可能为NULL。如果当时设置的timeout是0的话，永久等待，timer就是NULL。
        // 万万不可在此free timer，因为大根堆中还持有timer，还需要timer判断是否失效
        scheduler->epollElems[fd].timer = NULL;
    }

    scheduler->epollElemsSize--;
}


void WcoDestroyScheduler(WcoScheduler *scheduler){
    assert(scheduler);
    WcoHeapDestroy(scheduler->heap);
    WcoQueueDestroy(scheduler->pendingCoQueue);
    free(scheduler);
}

