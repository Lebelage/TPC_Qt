cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

project(TPC_Slint VERSION 0.1 LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TPC_SLINT_EXPORT_COMPILE_COMMANDS "Generate compile_commands.json" ON)
option(TPC_SLINT_COMPACT_RELEASE "Optimize Release builds for minimum size" OFF)
option(TPC_SLINT_BUILD_SCIENTIFIC_TESTS "Build deterministic scientific validation tests" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS "${TPC_SLINT_EXPORT_COMPILE_COMMANDS}")

find_package(nlohmann_json REQUIRED)
find_package(Slint 1.18 QUIET)

if(NOT Slint_FOUND)
    message(STATUS "Slint was not found locally; fetching Slint 1.18.1")
    set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_TESTING OFF CACHE BOOL "" FORCE)
    set(SLINT_STYLE fluent CACHE STRING "" FORCE)
    FetchContent_Declare(
            Slint
            GIT_REPOSITORY https://github.com/slint-ui/slint.git
            GIT_TAG v1.18.1
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR api/cpp
    )
    FetchContent_MakeAvailable(Slint)
endif()

option(TPC_SLINT_USE_INSTALLED_TPC "Require an installed TPC CMake package" OFF)
set(TPC_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/tpc_api"
        CACHE PATH "Path to the TPC_API source tree")

if(TPC_SLINT_USE_INSTALLED_TPC)
    find_package(TPC CONFIG REQUIRED)
elseif(TPC_API_DIR)
    if(NOT EXISTS "${TPC_API_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "TPC_API_DIR does not contain CMakeLists.txt: '${TPC_API_DIR}'")
    endif()
    if(NOT EXISTS "${TPC_API_DIR}/include/tpc/system/models/telemetry.hpp"
       OR NOT EXISTS "${TPC_API_DIR}/include/tpc/analytics/models/field_quality.hpp")
        message(FATAL_ERROR
            "TPC_API checkout is outdated/incomplete. Run git submodule update --init --recursive. "
            "If subprojects/tpc_api is an ordinary non-empty folder, preserve it and run "
            "scripts/repair-submodule.ps1 on Windows. See README.md. Do not use the API master branch; "
            "use the commit pinned by this repository.")
    endif()
    block()
        set(BUILD_SHARED_LIBS OFF)
        add_subdirectory("${TPC_API_DIR}" "${CMAKE_BINARY_DIR}/TPC_API" EXCLUDE_FROM_ALL)
    endblock()
endif()

if(TPC_SLINT_BUILD_SCIENTIFIC_TESTS)
    enable_testing()
    add_executable(tpc_scientific_tests tests/scientific_tests.cpp)
    target_include_directories(tpc_scientific_tests PRIVATE src)
    target_compile_definitions(tpc_scientific_tests PRIVATE TPC_TEST_REFERENCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/reference_map.json")
    target_link_libraries(tpc_scientific_tests PRIVATE TPC::TPC nlohmann_json::nlohmann_json)
    add_test(NAME scientific_validation COMMAND tpc_scientific_tests)
    add_executable(tpc_logging_tests tests/logging_tests.cpp
        src/services/logging/async_file_logger.cpp src/services/logging/logging_service.cpp
        src/services/event_dispatcher/event_dispatcher.cpp)
    target_include_directories(tpc_logging_tests PRIVATE src)
    target_link_libraries(tpc_logging_tests PRIVATE TPC::TPC nlohmann_json::nlohmann_json)
    add_test(NAME logging_validation COMMAND tpc_logging_tests)
endif()

set(TPC_SLINT_SOURCES
        src/main.cpp
        src/application/application_context.cpp
        src/application/application_context.hpp
        src/application/platform_paths.cpp
        src/application/platform_paths.hpp
        src/models/application_settings_model.hpp
        src/models/field_slice_model.hpp
        src/models/tpc_data_model.hpp
        src/services/event_dispatcher/event_dispatcher.cpp
        src/services/event_dispatcher/event_dispatcher.hpp
        src/services/field_slice/field_slice_service.cpp
        src/services/field_slice/field_slice_service.hpp
        src/services/file_worker/file_worker.cpp
        src/services/file_worker/file_worker.hpp
        src/services/logging/async_file_logger.cpp
        src/services/logging/async_file_logger.hpp
        src/services/logging/logging_service.cpp
        src/services/logging/logging_service.hpp
        src/services/scoped_subscription.hpp
        src/services/settings_holder/settings_holder.cpp
        src/services/settings_holder/settings_holder.hpp
        src/services/tpc_service/tpc_service.cpp
        src/services/tpc_service/tpc_service.hpp
        src/ui/application_view_binding.cpp
        src/ui/application_view_binding.hpp
        src/ui/file_dialog.hpp
        src/viewmodels/application_view_model.cpp
        src/viewmodels/application_view_model.hpp
)

if(APPLE)
    enable_language(OBJCXX)
    list(APPEND TPC_SLINT_SOURCES src/ui/file_dialog_macos.mm)
elseif(WIN32)
    list(APPEND TPC_SLINT_SOURCES src/ui/file_dialog_windows.cpp)
else()
    list(APPEND TPC_SLINT_SOURCES src/ui/file_dialog_fallback.cpp)
endif()

add_executable(TPC_Slint ${TPC_SLINT_SOURCES})
slint_target_sources(TPC_Slint ui/app.slint)

set_target_properties(TPC_Slint PROPERTIES
        CXX_SCAN_FOR_MODULES OFF
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.tpc.controller"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

target_include_directories(TPC_Slint PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(TPC_Slint PRIVATE Slint::Slint TPC::TPC nlohmann_json::nlohmann_json)

if(APPLE)
    target_link_libraries(TPC_Slint PRIVATE
            "-framework AppKit"
            "-framework UniformTypeIdentifiers")
elseif(WIN32)
    target_link_libraries(TPC_Slint PRIVATE ole32 shell32 uuid)
endif()

if(TARGET open62541pp::open62541pp)
    target_link_libraries(TPC_Slint PRIVATE open62541pp::open62541pp)
endif()
if(TARGET open62541::open62541)
    target_link_libraries(TPC_Slint PRIVATE open62541::open62541)
endif()

if(TPC_SLINT_COMPACT_RELEASE)
    set_property(TARGET TPC_Slint PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    if(MSVC)
        target_compile_options(TPC_Slint PRIVATE $<$<CONFIG:Release>:/O1> $<$<CONFIG:Release>:/Gy>)
        target_link_options(TPC_Slint PRIVATE $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
    else()
        target_compile_options(TPC_Slint PRIVATE
                $<$<CONFIG:Release>:-Os>
                $<$<CONFIG:Release>:-ffunction-sections>
                $<$<CONFIG:Release>:-fdata-sections>)
        if(APPLE)
            target_link_options(TPC_Slint PRIVATE $<$<CONFIG:Release>:-Wl,-dead_strip>)
        else()
            target_link_options(TPC_Slint PRIVATE $<$<CONFIG:Release>:-Wl,--gc-sections>)
        endif()
    endif()
endif()

if(WIN32)
    set_target_properties(TPC_Slint PROPERTIES WIN32_EXECUTABLE TRUE)
    if(TARGET slint_cpp-shared)
        add_custom_command(
                TARGET TPC_Slint POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:slint_cpp-shared>"
                        "$<TARGET_FILE_DIR:TPC_Slint>"
                COMMENT "Copying Slint runtime beside TPC_Slint"
                VERBATIM
        )
    endif()
endif()

if(MSVC)
    target_compile_options(TPC_Slint PRIVATE /bigobj /Zc:__cplusplus /Zc:preprocessor /permissive-)
    if(TARGET tpc_system)
        target_compile_options(tpc_system PRIVATE /bigobj /Zc:__cplusplus /Zc:preprocessor)
    endif()
endif()

if(WIN32)
    install(
            TARGETS TPC_Slint
            RUNTIME_DEPENDENCY_SET TPC_Slint_Runtime_Dependencies
            BUNDLE DESTINATION .
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    set(TPC_RUNTIME_DEPENDENCY_DIRS "${CMAKE_BINARY_DIR}")
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        list(APPEND TPC_RUNTIME_DEPENDENCY_DIRS
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    endif()

    install(
            RUNTIME_DEPENDENCY_SET TPC_Slint_Runtime_Dependencies
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            DIRECTORIES ${TPC_RUNTIME_DEPENDENCY_DIRS}
            PRE_EXCLUDE_REGEXES
                "api-ms-.*"
                "ext-ms-.*"
            POST_EXCLUDE_REGEXES
                ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss]ystem32[/\\\\].*"
    )
else()
    install(TARGETS TPC_Slint BUNDLE DESTINATION . RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(APPLE)
    # Copy non-system dylibs into the application bundle and rewrite their
    # install names so the installed .app is independent of the build tree.
    install(CODE "
        include(BundleUtilities)
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        fixup_bundle(
            \"\${CMAKE_INSTALL_PREFIX}/TPC_Slint.app\"
            \"\"
            \"${CMAKE_BINARY_DIR}/TPC_API\"
        )
    ")
endif()
