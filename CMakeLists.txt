cmake_minimum_required(VERSION 3.21)
project(OS_HW3 C)

set(CMAKE_C_STANDARD 11)

include_directories(webserver-files)

add_executable(OS_HW3
        webserver-files/request.c
        webserver-files/request.h
        webserver-files/segel.c
        webserver-files/segel.h
        webserver-files/server.c)
