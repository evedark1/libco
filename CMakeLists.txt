cmake_minimum_required(VERSION 3.0)
project(libco)

# Set cflags
set(CMAKE_C_FLAGS "-g -O2 -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT")
set(CMAKE_CXX_FLAGS "-g -O2 -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT")

# Use c and asm
enable_language(C ASM)

# Add source files
set(SOURCE_FILES
        co_comm.cpp
        co_epoll.cpp
        co_hook_sys_call.cpp
        co_routine.cpp
        coctx.cpp
        coctx_swap.S)

# Add static and shared library target
add_library(colib_static STATIC ${SOURCE_FILES})

# Macro for add example target
macro(add_example_target EXAMPLE_TARGET)
    add_executable("example_${EXAMPLE_TARGET}" "example_${EXAMPLE_TARGET}.cpp")
    target_link_libraries("example_${EXAMPLE_TARGET}" colib_static pthread dl)
endmacro(add_example_target)

add_example_target(closure)
add_example_target(cond)
add_example_target(copystack)
add_example_target(echocli)
add_example_target(echosvr)
add_example_target(poll)
add_example_target(setenv)
add_example_target(specific)
add_example_target(thread)
