cmake_minimum_required(VERSION 3.20)
project(Relinker CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(core/domain)
add_subdirectory(core/elfpatcher)
add_subdirectory(core/io)
add_subdirectory(core/relinker)
