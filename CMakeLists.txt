cmake_minimum_required(VERSION 4.1)
project(Home_Automation_Hub)

set(CMAKE_CXX_STANDARD 20)

add_executable(Home_Automation_Hub main.cpp
        interface/bridgeconnection.cpp
        interface/bridgeconnection.h
        config/configloader.cpp
        config/configloader.h
        modules/cameras/cameras.cpp
        modules/cameras/cameras.h
        modules/cameras/cameraautomation.cpp
        modules/cameras/cameraautomation.h
        modules/lights/lights.cpp
        modules/lights/lights.h
        modules/routines/routinemanager.cpp
        modules/routines/routinemanager.h
        threading/threadmanager.cpp
        threading/threadmanager.h
)
