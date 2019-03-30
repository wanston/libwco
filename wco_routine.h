//
// Created by tong on 19-3-30.
//

#ifndef LIBWCO_WCO_ROUTINE_H
#define LIBWCO_WCO_ROUTINE_H
#include <cstddef>


struct WcoStack;
typedef void (*WcoFn)(void*);

struct WcoRoutine {
    WcoStack *stack;
    WcoFn fn;
    void* reg[9];
};






/*
 * 为了避免占用栈空间，最好从堆上分配空间
 * */
//WcoRoutine* WcoCreate(WcoFn* fn, void *arg);

void WcoResume(WcoRoutine* co);

void WcoYield();

WcoStack *WcoCreateStack(size_t size);

#endif //LIBWCO_WCO_ROUTINE_H
