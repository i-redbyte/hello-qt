cmake_minimum_required(VERSION 3.16)
project(HelloQtNet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(HelloQtNet MACOSX_BUNDLE main.cpp)

target_link_libraries(HelloQtNet PRIVATE Qt6::Widgets Qt6::Network)
