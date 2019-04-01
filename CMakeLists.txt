cmake_minimum_required(VERSION 2.8)

enable_language(C ASM)

set(SOURCE_FILES
        wco_routine.c
        wco_swap.S)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(example example.c ${SOURCE_FILES})
