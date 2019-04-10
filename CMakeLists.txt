cmake_minimum_required(VERSION 2.8)

enable_language(C ASM)

set(SOURCE_C_FILES
        wco_routine.c
        wco_scheduler.c
        wco_tools.c
        wco_hook_sys_call.c)

set(SOURCE_ASM_FILES ./wco_swap.S)

set(SOURCE_FILES "${SOURCE_C_FILES};${SOURCE_ASM_FILES}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE ") # -DDEBUG

include_directories(include)

macro(add_example_target EXAMPLE_TARGET)
    add_executable("example_${EXAMPLE_TARGET}" "example_${EXAMPLE_TARGET}.c" ${SOURCE_FILES})
    target_link_libraries("example_${EXAMPLE_TARGET}" pthread dl)
endmacro(add_example_target)

macro(add_test_target TEST_TARGET)
    add_executable("test_${TEST_TARGET}" "test_${TEST_TARGET}.c" ${SOURCE_FILES})
    target_link_libraries("test_${TEST_TARGET}" pthread dl)
endmacro(add_test_target)

add_example_target(routine)
add_example_target(scheduler)
add_example_target(server)
add_test_target(tools)


add_library(wco STATIC ${SOURCE_FILES})
target_link_libraries(wco dl)
install(TARGETS wco ARCHIVE DESTINATION /home/tong/Project/开发/WebServerBasedLibwo/lib)
install(DIRECTORY include/ DESTINATION /home/tong/Project/开发/WebServerBasedLibwo/include/libwco)