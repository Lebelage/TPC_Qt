cmake_minimum_required(VERSION 4.4)

project(TPC_Qt
        VERSION 0.1
        LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Concurrent
        Gui
        Qml
        Quick
        QuickControls2
        Widgets
)

find_package(nlohmann_json REQUIRED)

qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# ============================================================
# C++ sources
# ============================================================

file(GLOB_RECURSE CPP_SOURCES
        CONFIGURE_DEPENDS

        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# QML resource paths must be relative to the source directory.

file(GLOB_RECURSE QML_FILES
        CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"

        "qml/*.qml"
)

# ============================================================
# TPC API
# ============================================================

# Development builds can consume a TPC_API source checkout. Packaged/CI builds
# can instead provide an installed TPC package through CMAKE_PREFIX_PATH.
option(TPC_QT_USE_INSTALLED_TPC "Require an installed TPC CMake package" OFF)
set(TPC_API_DIR "" CACHE PATH "Optional path to the TPC_API source tree")

if(NOT TPC_QT_USE_INSTALLED_TPC AND NOT TPC_API_DIR)
    get_filename_component(_tpc_home "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set(_tpc_api_candidates
            "$ENV{TPC_API_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../TPC_API"
            "${_tpc_home}/dev/projects/TPC_API"
    )

    foreach(_candidate IN LISTS _tpc_api_candidates)
        if(_candidate AND EXISTS "${_candidate}/CMakeLists.txt")
            set(TPC_API_DIR "${_candidate}" CACHE PATH "Optional path to the TPC_API source tree" FORCE)
            break()
        endif()
    endforeach()
endif()

if(TPC_QT_USE_INSTALLED_TPC)
    find_package(TPC CONFIG REQUIRED)
elseif(TPC_API_DIR)
    if(NOT EXISTS "${TPC_API_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "TPC_API_DIR does not contain CMakeLists.txt: '${TPC_API_DIR}'")
    endif()
    add_subdirectory("${TPC_API_DIR}" "${CMAKE_BINARY_DIR}/TPC_API" EXCLUDE_FROM_ALL)
else()
    find_package(TPC CONFIG REQUIRED)
endif()

# ============================================================
# Executable
# ============================================================

qt_add_executable(TPC_Qt
        ${CPP_SOURCES}
)

set_target_properties(TPC_Qt PROPERTIES
        CXX_SCAN_FOR_MODULES OFF
)

# ============================================================
# QML module
# ============================================================

qt_add_qml_module(TPC_Qt
        URI TPC
        VERSION 1.0
        SOURCES
        ${CPP_SOURCES}

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

# Keep Qt's convenience keywords from colliding with third-party APIs (oneTBB
# declares a member function named `emit`). Use Q_SIGNALS/Q_SLOTS/Q_EMIT in
# project sources instead.
target_compile_definitions(TPC_Qt PRIVATE QT_NO_KEYWORDS)

# ============================================================
# Libraries
# ============================================================

target_link_libraries(TPC_Qt
        PRIVATE
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets

        TPC::TPC

        nlohmann_json::nlohmann_json
)

# ============================================================
# macOS
# ============================================================

set_target_properties(TPC_Qt PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.tpc.controller"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        WIN32_EXECUTABLE TRUE
)

# ============================================================
# MSVC
# ============================================================

if(MSVC)
    target_compile_options(TPC_Qt
            PRIVATE
            /Zc:preprocessor
            /permissive-
    )
endif()

# ============================================================
# Installation and Qt runtime deployment
# ============================================================

install(TARGETS TPC_Qt
        BUNDLE DESTINATION .
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

qt_generate_deploy_qml_app_script(
        TARGET TPC_Qt
        OUTPUT_SCRIPT qt_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT "${qt_deploy_script}")
