cmake_minimum_required(VERSION 3.16)
project(qt_netmon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

add_executable(qt_netmon
        main.cpp

        core/ProbeResult.h
        core/INetProbe.h
        core/TcpConnectProbe.h
        core/TcpConnectProbe.cpp
        core/HttpHeadProbe.h
        core/HttpHeadProbe.cpp
        core/StatsCalculator.h
        core/StatsCalculator.cpp
        core/MonitorController.h
        core/MonitorController.cpp

        utils/StatusBadge.h
        utils/StatusBadge.cpp

        widgets/NetworkMonitorWidget.h
        widgets/NetworkMonitorWidget.cpp
        )

target_include_directories(qt_netmon PRIVATE .)

target_link_libraries(qt_netmon PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        )

if (APPLE)
    set_target_properties(qt_netmon PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.qt_netmon"
            MACOSX_BUNDLE_BUNDLE_NAME "Qt Network Monitor"
            )
endif()
