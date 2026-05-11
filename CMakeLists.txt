# github runner runs on cmake 3.31+, so keep this low.
cmake_minimum_required(VERSION 3.20)

project(batteryLoggerCpp)

set(CMAKE_CXX_STANDARD 20)

add_executable(batteryLoggerCpp WIN32 main.cpp resource.rc)
target_compile_definitions(batteryLoggerCpp PRIVATE UNICODE _UNICODE)
target_link_libraries(batteryLoggerCpp PRIVATE iphlpapi)
set_property(TARGET batteryLoggerCpp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
