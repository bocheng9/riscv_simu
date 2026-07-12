cmake_minimum_required(VERSION 4.2)
project(RVSIMU_V3)

set(CMAKE_CXX_STANDARD 20)

add_executable(RVSIMU_V3 main.cpp
        utils.cpp
        utils.h
        bus.cpp
        bus.h
        cpu.cpp
        cpu.h)
