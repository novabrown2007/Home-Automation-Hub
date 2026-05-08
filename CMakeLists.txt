cmake_minimum_required(VERSION 3.16)
project(Home_Automation_Hub)

set(CMAKE_CXX_STANDARD 20)
enable_testing()

set(HUB_SOURCES
        accesspoint/api.cpp
        accesspoint/api.h
        accesspoint/server.cpp
        accesspoint/server.h
        logging/logger.cpp
        logging/logger.h
        network/httpclient.cpp
        network/httpclient.h
        notifications/bridgenotifier.cpp
        notifications/bridgenotifier.h
        notifications/notifications.cpp
        notifications/notifications.h
        interface/bridgeconnection.cpp
        interface/bridgeconnection.h
        config/configloader.cpp
        config/configloader.h
        modules/cameras/cameras.cpp
        modules/cameras/cameras.h
        modules/cameras/cameraautomation.cpp
        modules/cameras/cameraautomation.h
        modules/cameras/motiondetector.cpp
        modules/cameras/motiondetector.h
        modules/lights/lights.cpp
        modules/lights/lights.h
        modules/routines/routinemanager.cpp
        modules/routines/routinemanager.h
        threading/threadmanager.cpp
        threading/threadmanager.h
)

add_library(HomeAutomationHubLib
        ${HUB_SOURCES}
)

add_executable(Home_Automation_Hub main.cpp)

target_link_libraries(Home_Automation_Hub HomeAutomationHubLib)
target_link_libraries(HomeAutomationHubLib ws2_32)
target_link_libraries(Home_Automation_Hub ws2_32)

add_executable(HomeAutomationHubTests
        tests/test_main.cpp
)

target_link_libraries(HomeAutomationHubTests HomeAutomationHubLib ws2_32)

add_test(NAME HomeAutomationHubTests COMMAND HomeAutomationHubTests)
