cmake_minimum_required(VERSION 3.16)
project(Home_Automation_Hub)

set(CMAKE_CXX_STANDARD 20)
enable_testing()

include(FetchContent)
find_package(nlohmann_json 3.11 QUIET)
if (NOT nlohmann_json_FOUND)
    FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

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
        bridge/hubProtocol.cpp
        bridge/hubProtocol.h
        bridge/hubClient.cpp
        bridge/hubClient.h
        bridge/subscriptions/subscriptionManager.cpp
        bridge/subscriptions/subscriptionManager.h
        bridge/commands/commandBuilder.cpp
        bridge/commands/commandBuilder.h
        bridge/state/bridgeStateCache.cpp
        bridge/state/bridgeStateCache.h
        bridge/streams/streamRegistry.cpp
        bridge/streams/streamRegistry.h
        bridge/handlers/deviceStateHandler.cpp
        bridge/handlers/deviceStateHandler.h
        bridge/handlers/deviceEventHandler.cpp
        bridge/handlers/deviceEventHandler.h
        bridge/handlers/deviceTelemetryHandler.cpp
        bridge/handlers/deviceTelemetryHandler.h
        bridge/handlers/streamHandler.cpp
        bridge/handlers/streamHandler.h
        bridge/handlers/analysisHandler.cpp
        bridge/handlers/analysisHandler.h
        bridge/handlers/bridgeErrorHandler.cpp
        bridge/handlers/bridgeErrorHandler.h
        bridge/routing/messageRouter.cpp
        bridge/routing/messageRouter.h
        config/configloader.cpp
        config/configloader.h
        modules/cameras/cameras.cpp
        modules/cameras/cameras.h
        modules/cameras/cameraautomation.cpp
        modules/cameras/cameraautomation.h
        modules/cameras/motiondetector.cpp
        modules/cameras/motiondetector.h
        modules/cameras/sounddetector.cpp
        modules/cameras/sounddetector.h
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
target_link_libraries(HomeAutomationHubLib ws2_32 gdiplus ole32)
target_link_libraries(HomeAutomationHubLib nlohmann_json::nlohmann_json)
target_link_libraries(Home_Automation_Hub ws2_32 gdiplus ole32)

if(WIN32)
    set(AURA_EXECUTABLE_DIR "C:/Users/novab/Desktop/Aura")

    add_executable(Home_Automation_Hub_Aura main.cpp)
    target_link_libraries(Home_Automation_Hub_Aura HomeAutomationHubLib ws2_32 gdiplus ole32)
    set_target_properties(Home_Automation_Hub_Aura PROPERTIES
            OUTPUT_NAME "Home_Automation_Hub"
            RUNTIME_OUTPUT_DIRECTORY "${AURA_EXECUTABLE_DIR}"
    )

    add_custom_command(TARGET Home_Automation_Hub_Aura PRE_LINK
            COMMAND ${CMAKE_COMMAND} -E make_directory "${AURA_EXECUTABLE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    "${CMAKE_SOURCE_DIR}/Home_Automation_Hub.exe"
                    "$<TARGET_FILE:Home_Automation_Hub_Aura>"
            COMMENT "Removing old Home Automation Hub executables before Aura export"
    )

    if(MINGW)
        get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET Home_Automation_Hub_Aura POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MINGW_RUNTIME_DIR}/libgcc_s_seh-1.dll"
                        "${AURA_EXECUTABLE_DIR}/libgcc_s_seh-1.dll"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MINGW_RUNTIME_DIR}/libstdc++-6.dll"
                        "${AURA_EXECUTABLE_DIR}/libstdc++-6.dll"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MINGW_RUNTIME_DIR}/libwinpthread-1.dll"
                        "${AURA_EXECUTABLE_DIR}/libwinpthread-1.dll"
                COMMENT "Copying MinGW runtime DLLs for Aura export"
        )
    endif()
endif()

add_executable(HomeAutomationHubTests
        tests/test_main.cpp
)

target_link_libraries(HomeAutomationHubTests HomeAutomationHubLib ws2_32 gdiplus ole32)

add_test(NAME HomeAutomationHubTests COMMAND HomeAutomationHubTests)
