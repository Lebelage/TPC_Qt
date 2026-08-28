cmake_minimum_required(VERSION 4.4)

project(TPC_Qt
        VERSION 0.1
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2
)

qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)

# ============================================================
# C++ sources
# ============================================================

file(GLOB_RECURSE CPP_SOURCES
        CONFIGURE_DEPENDS

        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# ============================================================
# QML sources
# ВАЖНО: пути делаем относительными, иначе Qt resource system
# ругается на absolute path.
# ============================================================

file(GLOB_RECURSE QML_FILES
        CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"

        "qml/*.qml"
)

# ============================================================
# TPC API
# ============================================================

add_subdirectory(
        /Users/sonora/dev/projects/TPC_API
        ${CMAKE_BINARY_DIR}/TPC_API
)

# ============================================================
# Executable
# ============================================================

qt_add_executable(TPC_Qt
        ${CPP_SOURCES}
        src/services/tpc_service/tpc_service.hpp
        src/services/tpc_service/tpc_service.cpp
)

# ============================================================
# QML module
# ============================================================

qt_add_qml_module(TPC_Qt
        URI TPC
        VERSION 1.0

        QML_FILES
        ${QML_FILES}
)

# ============================================================
# Include directories
# ============================================================

target_include_directories(TPC_Qt
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ============================================================
# Libraries
# ============================================================

target_link_libraries(TPC_Qt
        PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2

        tpc_system
)

# ============================================================
# macOS
# ============================================================

if(APPLE)
    set_target_properties(TPC_Qt PROPERTIES
            MACOSX_BUNDLE TRUE
    )
endif()