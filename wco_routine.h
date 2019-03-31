//
// Created by tong on 19-3-30.
//

#ifndef LIBWCO_WCO_ROUTINE_H
#define LIBWCO_WCO_ROUTINE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

struct WcoEnv_t;
struct WcoStack_t;
struct WcoRoutine_t;

typedef struct WcoEnv_t WcoEnv;
typedef struct WcoStack_t WcoStack;
typedef struct WcoRoutine_t WcoRoutine;
typedef void (*WcoFn)(void*);


struct WcoStack_t{
    // 需要保存的信息有：
    // 1. 分配的空间的指针的大小
    // 2. 栈底

    void *buffer;
    size_t bufferSize;

    // buffer + buffer_sz是协程栈帧的高地址边界。
    // align_highptr是buffer + buffer_sz 按照16字节向下对齐得到的地址
    // 模拟的是，函数参数刚压完栈的状态
    void *alignedHighPtr;
    bool guardPageEnabled;
};


struct WcoEnv_t{
    u_int64_t fpucwMxcsr = 0;
    WcoRoutine* callStack[128];
    int cur;
};


struct WcoRoutine_t{
    WcoStack *stack;
    WcoFn fn;
    void* reg[9];
    WcoEnv *env;
};


WcoRoutine* WcoCreate(WcoStack *sharedStack, WcoFn fn, void *arg);
void WcoResume(WcoRoutine* co);
void WcoYield();
WcoStack *WcoCreateStack(size_t size, bool guardPageEnabled);

#endif //LIBWCO_WCO_ROUTINE_H
