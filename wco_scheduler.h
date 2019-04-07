//
// Created by tong on 19-4-2.
//

#ifndef PROJECT_WCO_SCHEDULER_H
#define PROJECT_WCO_SCHEDULER_H

#include "wco_tools.h"
#include "wco_routine.h"
#include "wco_tools.h"

#define MAX_SIZE 1024*10

struct WcoEpollElem_t {
    int fd;
    WcoRoutine* co;
    int events;
    long timeout;
    WcoHeapNode *timer;
};

typedef struct WcoEpollElem_t WcoEpollElem;


// 采用epollElems数组，用fd作为索引，这种方式有限制
// 如果两个协程，同时对同一个fd进行操作，这种情况就会引发bug
struct WcoScheduler_t{
    WcoQueue *pendingCoQueue; // 这是需要主动resume的协程
    int epollFd;
    WcoEpollElem epollElems[MAX_SIZE];
    size_t epollElemsSize; // 在系统调用中，把元素降低


    WcoRoutine *activeCos[MAX_SIZE]; // 待reusme的协程
    size_t activeSize;

    size_t runningSize; // 调度器中的协程数目
    WcoBigRootHeap *heap;
};

typedef struct WcoScheduler_t WcoScheduler;

// 被添加进WcoScheduler的协程，在执行完后，都会自动被destroy
WcoScheduler* WcoGetScheduler();
void WcoAddCoToScheduler(WcoScheduler* , WcoRoutine* );
void WcoRunScheduler(WcoScheduler *);
void WcoDestroyScheduler(WcoScheduler *);

#endif //PROJECT_WCO_SCHEDULER_H
