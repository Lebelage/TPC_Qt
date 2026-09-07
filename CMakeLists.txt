cmake_minimum_required(VERSION 4.4)

project(TPC_Qt
        VERSION 0.1
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2
)

find_package(nlohmann_json)

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

file(GLOB_RECURSE CXX_MODULES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ixx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm"
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
        "C:/Users/excep/CLionProjects/TPC_API"
        #/Users/sonora/dev/projects/TPC_API
        #"${CMAKE_BINARY_DIR}/TPC_API"
        ${CMAKE_BINARY_DIR}/TPC_API
)

# ============================================================
# Executable
# ============================================================

qt_add_executable(TPC_Qt
        ${CPP_SOURCES}
)

target_sources(TPC_Qt
        PRIVATE
        FILE_SET cxx_modules
        TYPE CXX_MODULES
        BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES
        ${CXX_MODULES}
)

set_target_properties(TPC_Qt PROPERTIES
        CXX_SCAN_FOR_MODULES ON
)

# ============================================================
# QML module
# ============================================================

qt_add_qml_module(TPC_Qt
        URI TPC
        VERSION 1.0
        # Обязательно передаем C++ файлы сюда для корректной генерации moc/qml-регистраций
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

        nlohmann_json::nlohmann_json
)

# ============================================================
# macOS
# ============================================================

if(APPLE)
    set_target_properties(TPC_Qt PROPERTIES
            MACOSX_BUNDLE TRUE
    )
endif()

if(WIN32)
    set_target_properties(TPC_Qt PROPERTIES
            WIN32_EXECUTABLE FALSE
    )
endif()

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
# Windows runtime DLL deployment
#
# Копирует DLL зависимостей, которые CMake видит как targets.
# Сюда обычно попадают vcpkg DLL:
#
#   open62541.dll
#   open62541pp.dll
#   и т.д.
# ============================================================

if(WIN32)

    add_custom_command(
            TARGET TPC_Qt
            POST_BUILD

            COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:TPC_Qt>
            $<TARGET_FILE_DIR:TPC_Qt>

            COMMAND_EXPAND_LISTS

            COMMENT
            "Copying runtime DLL dependencies..."
    )

endif()

# ============================================================
# Qt deployment
# ============================================================

if(WIN32)

    # Qt предоставляет путь к windeployqt через target Qt6::windeployqt.
    # Используем generator expression вместо захардкоженного
    # C:/Qt/6.xx/bin/windeployqt.exe.

    add_custom_command(
            TARGET TPC_Qt
            POST_BUILD

            COMMAND
            "$<TARGET_FILE:Qt6::windeployqt>"

            --dir "$<TARGET_FILE_DIR:TPC_Qt>"
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"

            "$<TARGET_FILE:TPC_Qt>"

            COMMENT
            "Deploying Qt runtime and QML dependencies..."
    )

endif()