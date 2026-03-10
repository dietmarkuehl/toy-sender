# /CMakeLists.txt -*-makefile-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.30...4.3)
project(toy-sender)

set (PROGRAMS main)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

foreach(prog ${PROGRAMS})
    add_executable(${prog} ${prog}.cpp)
    set_property(TARGET ${prog} PROPERTY CXX_STANDARD 23)
endforeach()
