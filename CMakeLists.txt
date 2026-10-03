cmake_minimum_required(VERSION 3.25)

project(TPC_Qt
        VERSION 0.1
        LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TPC_QT_EXPORT_COMPILE_COMMANDS "Generate compile_commands.json" ON)
option(TPC_QT_COMPACT_RELEASE "Optimize Release builds for minimum size" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS "${TPC_QT_EXPORT_COMPILE_COMMANDS}")

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

# The bundled source tree makes a checkout self-contained on every platform.
# An installed TPC package or another source checkout can still be selected for
# package-maintainer workflows.
option(TPC_QT_USE_INSTALLED_TPC "Require an installed TPC CMake package" OFF)
set(TPC_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/tpc_api"
        CACHE PATH "Path to the TPC_API source tree")

if(TPC_QT_USE_INSTALLED_TPC)
    find_package(TPC CONFIG REQUIRED)
elseif(TPC_API_DIR)
    if(NOT EXISTS "${TPC_API_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "TPC_API_DIR does not contain CMakeLists.txt: '${TPC_API_DIR}'")
    endif()
    add_subdirectory("${TPC_API_DIR}" "${CMAKE_BINARY_DIR}/TPC_API" EXCLUDE_FROM_ALL)
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

if(TPC_QT_COMPACT_RELEASE)
    set_property(TARGET TPC_Qt PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

    if(MSVC)
        target_compile_options(TPC_Qt PRIVATE
                $<$<CONFIG:Release>:/O1>
                $<$<CONFIG:Release>:/Gy>
        )
        target_link_options(TPC_Qt PRIVATE
                $<$<CONFIG:Release>:/OPT:REF>
                $<$<CONFIG:Release>:/OPT:ICF>
        )
    else()
        target_compile_options(TPC_Qt PRIVATE
                $<$<CONFIG:Release>:-Os>
                $<$<CONFIG:Release>:-ffunction-sections>
                $<$<CONFIG:Release>:-fdata-sections>
        )

        if(APPLE)
            target_link_options(TPC_Qt PRIVATE
                    $<$<CONFIG:Release>:-Wl,-dead_strip>
                    $<$<CONFIG:Release>:-Wl,-x>
            )
        else()
            target_link_options(TPC_Qt PRIVATE
                    $<$<CONFIG:Release>:-Wl,--gc-sections>
                    $<$<CONFIG:Release>:-Wl,--strip-all>
            )
        endif()
    endif()
endif()

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

# Keep the final link self-contained on Windows. TPC::TPC already exposes these
# dependencies transitively, but explicitly listing the imported targets avoids
# MSVC dropping them when TPC is consumed through its interface/static targets.
if(TARGET open62541pp::open62541pp)
    target_link_libraries(TPC_Qt PRIVATE open62541pp::open62541pp)
endif()

if(TARGET open62541::open62541)
    target_link_libraries(TPC_Qt PRIVATE open62541::open62541)
endif()

set_target_properties(TPC_Qt PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.tpc.controller"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

# Use the program's regular main() entry point. This also avoids depending on
# Qt's optional WinMain adapter, which is not present in every Windows Qt kit.
if(WIN32)
    set_target_properties(TPC_Qt PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

# ============================================================
# MSVC
# ============================================================

if(MSVC)
    target_compile_options(TPC_Qt
            PRIVATE
            /bigobj
            /Zc:__cplusplus
            /Zc:preprocessor
            /permissive-
    )

    # TPC API is bundled as a subproject. Keep its MSVC language mode aligned
    # with the application without carrying a platform-only fork of TPC API.
    if(TARGET tpc_system)
        target_compile_options(tpc_system PRIVATE
                /bigobj
                /Zc:__cplusplus
                /Zc:preprocessor
        )
    endif()
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
