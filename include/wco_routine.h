//
// Created by tong on 19-3-30.
//

#ifndef LIBWCO_WCO_ROUTINE_H
#define LIBWCO_WCO_ROUTINE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef __x86_64__
#error "libwco only support __x86_64__ platform."
#endif

struct WcoEnv_t;
struct WcoStack_t;
struct WcoRoutine_t;

typedef struct WcoEnv_t WcoEnv;
typedef struct WcoStack_t WcoStack;
typedef struct WcoRoutine_t WcoRoutine;
typedef void (*WcoFn)(void*);

struct WcoStack_t{
    void *buffer;
    size_t bufferSize;
    size_t stackSize; // 有用的大小，和alignedHighPtr相关
    // align_highptr是buffer + buffer_sz 按照16字节向下对齐得到的地址
    // 模拟的是，函数参数刚压完栈的状态
    void *alignedHighPtr;
    bool guardPageEnabled;
    bool isShared;
    WcoRoutine *owner;
};


struct WcoEnv_t{
    void* fpucwMxcsr;
    WcoRoutine* curCo;
    WcoRoutine* mainCo;
};


struct WcoRoutine_t{
    void* reg[10];
    WcoStack *stack;
    WcoFn fn;
    bool isEnd;
    void *saveBuffer;
    size_t saveBufferSize;
};


WcoRoutine* WcoCreate(WcoStack *sharedStack, WcoFn fn, void *arg);
void WcoDestroy(WcoRoutine* co);

WcoStack *WcoCreateStack(size_t size, bool guardPageEnabled);
void WcoDestroyStack(WcoStack* stack);

void WcoResume(WcoRoutine* co);
void WcoYield();

WcoRoutine *WcoGetCurrentCo();
#endif //LIBWCO_WCO_ROUTINE_H
