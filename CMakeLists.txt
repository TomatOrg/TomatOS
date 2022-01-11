cmake_minimum_required(VERSION 3.17)
project(pentagon C CXX)

set(CMAKE_C_STANDARD 11)

########################################################################################################################
# Compiling pentagon itself
########################################################################################################################

set(CMAKE_C_FLAGS "-O1 -g -Wformat -Werror -Wno-address-of-packed-member")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -flto -static -fshort-wchar")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-red-zone")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lgcc")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_C_FLAGS} -T${CMAKE_SOURCE_DIR}/kernel/linker.ld")

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.c ${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.h ${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.hpp)

add_compile_definitions(__PENTAGON_DEBUG__)

add_compile_definitions(PRINTF_NTOA_BUFFER_SIZE=64)
add_compile_definitions(PRINTF_DISABLE_SUPPORT_FLOAT)
add_compile_definitions(PRINTF_DISABLE_SUPPORT_EXPONENTIAL)

include_directories(lib)
include_directories(kernel)

add_compile_definitions(MIR_NO_SCAN)

set(MIR_SOURCES
    lib/mir/mir.c
    lib/mir/mir-gen.c
)

add_executable(pentagon ${SOURCES} ${MIR_SOURCES})
