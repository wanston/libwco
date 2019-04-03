cmake_minimum_required(VERSION 2.8)

enable_language(C ASM)

aux_source_directory(. SOURCE_C_FILES)
set(SOURCE_ASM_FILES ./wco_swap.S)
set(SOURCE_FILES "${SOURCE_C_FILES};${SOURCE_ASM_FILES}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(example example.c ${SOURCE_FILES})
