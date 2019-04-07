//
// Created by tong on 19-4-1.
//

#include <stdio.h>
#include "wco_routine.h"

WcoRoutine *a;
WcoRoutine *b;


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
    WcoStack* s = WcoCreateStack(0, true);

    a = WcoCreate(s, foo_a, NULL);
    b = WcoCreate(s, foo_b, NULL);
    WcoResume(a);
    WcoResume(b);
    WcoResume(b);
    WcoResume(a);
    WcoResume(a);
    WcoResume(b);

    WcoDestroy(a);
    WcoDestroy(b);
}
