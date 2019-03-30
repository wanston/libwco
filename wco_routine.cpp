//
// Created by tong on 19-3-30.
//

#include <cstddef>
#include <stdlib.h>
#include "wco_routine.h"
#include "wco_assert.h"

#define RETADDR
#define RSP
#define FPU
#define RDI
#define RSI


struct WcoStack{
    // 需要保存的信息有：
    // 1. 分配的空间的指针的大小
    // 2. 栈底
    void *buffer;
    size_t buffer_sz;

    // buffer + buffer_sz是协程栈帧的边界。
    // stackBottom = buffer + buffer_sz - sizeof(void*);
    // stackBottom位置存储的是retaddr。模拟的是协程函数的栈帧开始的地方。
    // 栈的高地址边界，模拟的是：函数参数刚压完栈时，再。需要16字节对齐。
    void *highBoundary;
};

void realWcoRoutineFn(WcoRoutine *wco, void *arg){
    wco_assertptr(wco);
    wco_assertptr(wco->fn);
    wco->fn(arg);
    WcoYield();
}



// sharedStack表示共享栈，其为null表示不使用共享栈，自己新建一个栈，栈大小为stackSize
// 不为null，则表示使用指定的共享栈，
WcoRoutine* WcoCreate(WcoStack *sharedStack, size_t stackSize, WcoFn fn, void *arg){
    wco_assertptr(fn);
    WcoStack *stack;

    if(!sharedStack){
        size_t sz = 1024 * 1024 * 2; // 2M
        stack = WcoCreateStack(sz);
    }else{
        stack = sharedStack;
    }




    WcoRoutine *wco = (WcoRoutine*)calloc(sizeof(WcoRoutine), 1);
    wco->stack = stack;
    wco->fn = fn;

    wco->reg[RETADDR] = (void*)realWcoRoutineFn;
    wco->reg[RSP] = stack->stackBottom;
    wco->reg[FPU] = ;
    wco->reg[RDI] = (void*)wco; // 第一参数
    wco->reg[RSI] = arg; // 第二参数

    return wco;
}




