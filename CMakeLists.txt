cmake_minimum_required(VERSION 2.8)

enable_language(C ASM)

set(SOURCE_C_FILES
        wco_routine.c
        wco_scheduler.c
        wco_tools.c
        wco_hook_sys_call.c)

set(SOURCE_ASM_FILES ./wco_swap.S)

set(SOURCE_FILES "${SOURCE_C_FILES};${SOURCE_ASM_FILES}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")

add_executable(example example.c ${SOURCE_FILES})
add_executable(example_server example_server.c ${SOURCE_FILES})
target_link_libraries(example_server pthread dl)
add_executable(tools_test tools_test.c ${SOURCE_FILES})
add_executable(scheduler_test scheduler_test.c ${SOURCE_FILES})
target_link_libraries(scheduler_test pthread dl)