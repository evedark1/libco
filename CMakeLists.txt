cmake_minimum_required(VERSION 3.0)
project(libco)

# Set cflags
set(CMAKE_C_FLAGS "-g -O0 -DNDEBUG -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT")
set(CMAKE_CXX_FLAGS "-g -O0 -std=c++11 -DNDEBUG -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT")

# Use c and asm
enable_language(C ASM)

# Add source files
set(SOURCE_FILES
        co_epoll.cpp
        co_routine.cpp
        coctx.c
        md_linux.S
        md_linux2.S)

# Add static and shared library target
add_library(colib_static STATIC ${SOURCE_FILES})

# Macro for add example target
macro(add_example_target EXAMPLE_TARGET)
    add_executable("example_${EXAMPLE_TARGET}" "example_${EXAMPLE_TARGET}.cpp")
    target_link_libraries("example_${EXAMPLE_TARGET}" colib_static pthread dl)
endmacro(add_example_target)

add_example_target(comem)
add_example_target(cond)
add_example_target(echocli)
add_example_target(echosvr)
add_example_target(switch)
add_example_target(order)
