//
// Created by tong on 19-3-30.
//

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdbool.h>
#ifdef DEBUG
#include <stdio.h>
#endif
#include "wco_routine.h"
#include "wco_assert.h"

#define RSP 0
#define FPU 7
#define RDI 8
#define RSI 9


static __thread WcoEnv *wcoEnv;

extern void WcoSwapContext(WcoRoutine* from_co, WcoRoutine* to_co) __asm__("WcoSwapContext"); // asm
extern void WcoSaveFpucwMxcsr(void* p) __asm__("WcoSaveFpucwMxcsr");  // asm

void PrepareSharedStack(WcoRoutine* co);

void WcoInitRoutineEnv(){
    wcoEnv = (WcoEnv*)calloc(sizeof(WcoEnv), 1);
    WcoSaveFpucwMxcsr(&(wcoEnv->fpucwMxcsr));

    WcoRoutine *mainCo = (WcoRoutine*)calloc(sizeof(WcoRoutine), 1);
    wcoEnv->curCo = mainCo;
    wcoEnv->mainCo = mainCo;
#ifdef DEBUG
    printf("main co %x\n", mainCo);
#endif
}


void realWcoRoutineFn(WcoRoutine *co, void *arg){
    wco_assertptr(co);
    wco_assertptr(co->fn);
    co->fn(arg);
    co->isEnd = true;
    WcoYield();
}

//void WcoProtector(){
//    fprintf(stderr,"WcoProtector!\n");
//    abort();
//}


// sharedStack表示共享栈，其为null表示不使用共享栈，自己新建一个栈，栈大小为stackSize
// 不为null，则表示使用指定的共享栈，
WcoRoutine* WcoCreate(WcoStack *sharedStack, WcoFn fn, void *arg){
    wco_assertptr(fn);

    if(!wcoEnv){
        WcoInitRoutineEnv();
    }

    WcoRoutine *wco = (WcoRoutine*)calloc(sizeof(WcoRoutine), 1);
    WcoStack *stack;

    if(!sharedStack){ // 如果不指定栈，就默认
        stack = WcoCreateStack(0, false);
        stack->isShared = false;
        stack->owner = wco;
    }else{
        if(sharedStack->owner){
            PrepareSharedStack(sharedStack->owner);
        }

        stack = sharedStack;
        stack->isShared = true;
        stack->owner = wco;
        size_t sz = 64;
        wco->saveBuffer = malloc(sz);
        wco->saveBufferSize = sz;
    }

    *(void**)(stack->alignedHighPtr - sizeof(void*)) = (void*)realWcoRoutineFn;
    wco->stack = stack;
    wco->fn = fn;
    wco->reg[RSP] = (char*)stack->alignedHighPtr - sizeof(void*);
    wco->reg[FPU] = wcoEnv->fpucwMxcsr;
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
    stack->stackSize = stack->alignedHighPtr-stack->buffer;
//    *(void**)(stack->alignedHighPtr - sizeof(void*)) = (void*)WcoProtector;
    return stack;
}


void WcoResume(WcoRoutine* to_co){
#ifdef DEBUG
    printf("resume co %x\n", to_co);
#endif
    assert(to_co != wcoEnv->mainCo);
    assert(!to_co->isEnd);
    assert(wcoEnv->curCo == wcoEnv->mainCo);

    if(to_co->isEnd){
        return;
    }

    if(to_co->stack->owner != to_co){
        PrepareSharedStack(to_co);
    }

    WcoRoutine* from_co = wcoEnv->curCo;
    wcoEnv->curCo = to_co;
    WcoSwapContext(from_co, to_co);
    wcoEnv->curCo = from_co;
}


void WcoYield(){
#ifdef DEBUG
    printf("yield to main co\n");
#endif
    assert(wcoEnv->curCo != wcoEnv->mainCo);
    WcoSwapContext(wcoEnv->curCo, wcoEnv->mainCo);
}


/*
 * 销毁协程。如果协程栈不是shared，一并销毁协程栈。
 * */
void WcoDestroy(WcoRoutine* co){
    if(!co->stack->isShared){
        WcoDestroyStack(co->stack);
    }else if(co->stack->owner == co){
        co->stack->owner = NULL;
    }

    free(co->saveBuffer);
    free(co);
}

/*
 * 销毁协程栈，无论是否仍有协程使用它。
 * */
void WcoDestroyStack(WcoStack* stack){
    if(stack->guardPageEnabled){
        assert(0 == munmap(stack->buffer, stack->bufferSize));
    }else{
        free(stack->buffer);
    }
    free(stack);
}


void PrepareSharedStack(WcoRoutine* co){
    // 保存old owner的栈内容
    WcoRoutine *oldOwner = co->stack->owner;
    size_t sz = oldOwner->stack->alignedHighPtr - oldOwner->reg[RSP];
    if(oldOwner->saveBufferSize < sz){
        free(oldOwner->saveBuffer);
        while(1){
            oldOwner->saveBufferSize = oldOwner->saveBufferSize << 1;
            if(oldOwner->saveBufferSize >= sz){
                break;
            }
        }
        oldOwner->saveBuffer = malloc(oldOwner->saveBufferSize);
        assert(oldOwner->saveBuffer);
    }
    memcpy(oldOwner->saveBuffer, oldOwner->reg[RSP], sz);
    // 加载new owner的栈内容
    sz = co->stack->alignedHighPtr - co->reg[RSP];
    memcpy(co->reg[RSP], co->saveBuffer, sz);

    co->stack->owner = co;
}


WcoRoutine *WcoGetCurrentCo(){
    return wcoEnv->curCo;
}