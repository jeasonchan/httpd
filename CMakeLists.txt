cmake_minimum_required(VERSION 3.16)
project(httpd)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(httpd
#        clitest.cpp
        common.h
        io.cpp
        io.h
        main.cpp
        net.cpp
        net.h
        ser.cpp
        ser.h)
