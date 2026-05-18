cmake_minimum_required(VERSION 3.20)
project(RemoteMonitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

find_package(OpenCV QUIET)

include_directories(${CMAKE_SOURCE_DIR}/include)

if(OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})

    # 核心主控进程
    add_executable(controller
        src/core/main.cpp
        src/core/PluginManager.cpp
        src/core/EventBus.cpp
        src/core/Config.cpp
    )
    target_link_libraries(controller ${OpenCV_LIBS})

    # 屏幕监控插件
    add_library(monitor_plugin SHARED
        src/monitor/MonitorPlugin.cpp
    )
    target_link_libraries(monitor_plugin ${OpenCV_LIBS})

    # 通信插件
    add_library(comm_plugin SHARED
        src/comm/WeChatComm.cpp
        src/comm/OCREngine.cpp
    )
    target_link_libraries(comm_plugin ${OpenCV_LIBS})

    # 操作插件
    add_library(action_plugin SHARED
        src/action/Win32Action.cpp
    )
else()
    message(STATUS "OpenCV not found - building tests only")
endif()

# ===== 测试 =====
enable_testing()

# 不依赖OpenCV的测试
add_executable(test_eventbus
    test/test_eventbus.cpp
    src/core/EventBus.cpp
)
target_include_directories(test_eventbus PRIVATE test)
add_test(NAME EventBus COMMAND test_eventbus)

add_executable(test_config
    test/test_config.cpp
    src/core/Config.cpp
)
target_include_directories(test_config PRIVATE test)
add_test(NAME Config COMMAND test_config)

add_executable(test_json
    test/test_json.cpp
)
target_include_directories(test_json PRIVATE test)
add_test(NAME Json COMMAND test_json)

add_executable(test_ipc
    test/test_ipc.cpp
    src/core/IPC.cpp
)
target_include_directories(test_ipc PRIVATE test)
add_test(NAME IPC COMMAND test_ipc)

add_executable(test_action
    test/test_action.cpp
)
target_include_directories(test_action PRIVATE test)
add_test(NAME Action COMMAND test_action)

# 依赖OpenCV的测试
if(OpenCV_FOUND)
    add_executable(test_monitor
        test/test_monitor.cpp
    )
    target_include_directories(test_monitor PRIVATE test)
    target_link_libraries(test_monitor ${OpenCV_LIBS})
    add_test(NAME Monitor COMMAND test_monitor)

    add_executable(test_comm
        test/test_comm.cpp
    )
    target_include_directories(test_comm PRIVATE test)
    target_link_libraries(test_comm ${OpenCV_LIBS})
    add_test(NAME Comm COMMAND test_comm)
endif()
