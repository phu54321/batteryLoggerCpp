cmake_minimum_required(VERSION 4.0)
project(batteryLoggerCpp)

set(CMAKE_CXX_STANDARD 20)

add_executable(batteryLoggerCpp WIN32 main.cpp resource.rc)
target_link_libraries(batteryLoggerCpp PRIVATE iphlpapi)