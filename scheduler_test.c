//
// Created by tong on 19-4-4.
//
#include <stdio.h>
#include "wco_scheduler.h"
#include "wco_routine.h"

void foo_a(void* p){
    printf("1\n");
    WcoYield();
    printf("2\n");
};


void foo_b(void *p){
    printf("a\n");
    WcoYield();
    printf("b\n");
}

int main(){
    WcoScheduler *s = WcoGetScheduler();

    WcoRoutine *coa = WcoCreate(NULL, foo_a, NULL);
    WcoRoutine *cob = WcoCreate(NULL, foo_b, NULL);

    WcoAddCoToScheduler(s, coa);
    WcoAddCoToScheduler(s, cob);
    WcoRunScheduler(s);

    WcoDestroyScheduler(s);
}