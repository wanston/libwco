//
// Created by tong on 19-4-5.
//

#ifndef PROJECT_WCO_HOOK_SYS_CALL_H
#define PROJECT_WCO_HOOK_SYS_CALL_H

#include <stdbool.h>

bool WcoHookIsEnabled(); // hook系统调用是什么层面的事情？暂时设计成协程层面的吧。

#endif //PROJECT_WCO_HOOK_SYS_CALL_H
