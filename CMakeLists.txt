cmake_minimum_required(VERSION 3.16)
project(RVSIMU_V3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(v3sim
        main.cpp
        cpu.cpp
        cpu.h
        bus.cpp
        bus.h
        utils.cpp
        utils.h
        dev.cpp
        dev.h
        rvc.cpp
        rvc.h
        plic.cpp
        plic.h)
