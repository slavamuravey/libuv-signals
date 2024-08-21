cmake_minimum_required(VERSION 3.10)
project(libuv-signals)
include_directories("include/")
add_compile_definitions(_GNU_SOURCE)

set(
    UV_SOURCES
    
    src/core.c  src/linux.c  src/loop.c  src/signal.c  src/uv-common.c src/pipe.c
)

add_executable(
    signals 
    
    ${UV_SOURCES}
    
    examples/signals/main.c
)
