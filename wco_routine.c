//
// Created by tong on 19-3-30.
//

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "wco_routine.h"
#include "wco_assert.h"

/*
 * swap 函数要实现的是：
 * 1. 保存from_co的8个callee负责的寄存器。此时rsp应指向retaddr
 * 2. 把to_co的8+2个寄存器值加载进来。之所以加两个，是为了在最初的时候，让封装的协程函数成功运行。
 * 3. ret
 * */


enum RegisterIdx{
    RSP = 0,
    RBP,
    RBX,
    R12,
    R13,
    R14,
    R15,
    FPU, // 前面是callee要保存的寄存器
    RDI,
    RSI, // 这两个传参用的寄存器，为了
//    RETADDR, // 除了要保存寄存器外，还要保存返回地址。
};

static __thread WcoEnv wcoEnv;

extern void* WcoSwapContext(WcoRoutine* from_co, WcoRoutine* to_co) __asm__("WcoSwapContext"); // asm
extern void WcoSaveFpucwMxcsr(void* p) __asm__("WcoSaveFpucwMxcsr");  // asm



void realWcoRoutineFn(WcoRoutine *wco, void *arg){
    wco_assertptr(wco);
    wco_assertptr(wco->fn);
    wco->fn(arg);
    WcoYield();
}

//void WcoProtector(){
//    fprintf(stderr,"WcoProtector!\n");
//    abort();
//}


// sharedStack表示共享栈，其为null表示不使用共享栈，自己新建一个栈，栈大小为stackSize
// 不为null，则表示使用指定的共享栈，
WcoRoutine* WcoCreate(WcoStack *sharedStack, WcoFn fn, void *arg){
    // 保存控制字
    if(!wcoFpucwMxcsr){
        WcoSaveFpucwMxcsr(&wcoFpucwMxcsr);
    }

    wco_assertptr(fn);
    WcoStack *stack;

    if(!sharedStack){ // 如果不指定栈，就默认
        size_t sz = 1024 * 1024 * 2; // 2M
        stack = WcoCreateStack(sz, false);
    }else{
        stack = sharedStack;
        // TODO： 实现共享栈的细节
    }

    *(void**)(stack->alignedHighPtr - sizeof(void*)) = (void*)realWcoRoutineFn;

    WcoRoutine *wco = (WcoRoutine*)calloc(sizeof(WcoRoutine), 1);
    wco->stack = stack;
    wco->fn = fn;
    wco->reg[RSP] = (char*)stack->alignedHighPtr - sizeof(void*);
    wco->reg[FPU] = wcoFpucwMxcsr;
    wco->reg[RDI] = (void*)wco; // 第一参数
    wco->reg[RSI] = arg; // 第二参数
    return wco;
}


WcoStack *WcoCreateStack(size_t size, bool guardPageEnabled){
    if(size <= 0){
        size = 1024*1024*2; // 2M
    }

    if(size  < 4096){
        size = 4096;
    }

    WcoStack *stack = (WcoStack*)malloc(sizeof(WcoStack));
    wco_assertptr(stack);
    memset(stack, 0, sizeof(WcoStack));

    if(guardPageEnabled){
        // 确定bufferSize的大小
        long pgsz = sysconf(_SC_PAGESIZE);

        if(size < pgsz){
            size = (size_t)(pgsz << 1);
        }else{
            size_t newSize;
            if((size & (pgsz - 1)) != 0){ // 不是page size整数倍
                newSize = (size & (~(pgsz - 1))); // new_sz = sz向下按page size取整
                newSize += (pgsz << 1); // new_sz += page_size*2，一个page取整，一个page作为guard。
            } else {
                newSize = size + pgsz;
            }
            size = newSize;
        }
        assert(size >= (pgsz << 1));
        // 分配空间
        stack->buffer = mmap(
                NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0
        );
        stack->bufferSize = size;
        stack->guardPageEnabled = true;
        assert(stack->buffer != MAP_FAILED);
        int ret = mprotect(stack->buffer, (size_t)pgsz, PROT_READ);
        assert(ret == 0);
    }else{
        // 直接分配空间
        stack->buffer = malloc(size);
        stack->bufferSize = size;
        assert(stack->buffer);
    }

    u_int64_t highPtr = (u_int64_t)stack->buffer + stack->bufferSize;
    highPtr = (highPtr >> 4) << 4; // 按照16字节对齐
    stack->alignedHighPtr = (void*)highPtr;

//    *(void**)(stack->alignedHighPtr - sizeof(void*)) = (void*)WcoProtector;
}


void WcoResume(WcoRoutine* co){

}


void WcoYield(){
    wcoCallStack[];
    WcoSwapContext();
}
