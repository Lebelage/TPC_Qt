cmake_minimum_required(VERSION 3.25)

project(
    TPC
    VERSION 0.1.0
    DESCRIPTION "TPC control and magnetic-field analytics library"
    LANGUAGES CXX
)

include(CTest)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(TPC_INSTALL "Generate install and CMake package targets" ${PROJECT_IS_TOP_LEVEL})
option(TPC_ENABLE_WARNINGS "Enable additional compiler warnings" ON)

find_package(Eigen3 CONFIG REQUIRED)
find_package(open62541pp CONFIG REQUIRED)
find_package(stdexec CONFIG REQUIRED)

set(TPC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(tpc_utilities INTERFACE)
add_library(TPC::Utilities ALIAS tpc_utilities)
set_target_properties(tpc_utilities PROPERTIES EXPORT_NAME Utilities)
target_compile_features(tpc_utilities INTERFACE cxx_std_23)
target_sources(
    tpc_utilities
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${TPC_INCLUDE_DIR}"
        FILES
            include/tpc/utilities/chunkview.hpp
            include/tpc/utilities/event_handler.hpp
            include/tpc/utilities/header_function.hpp
)

add_library(tpc_core INTERFACE)
add_library(TPC::Core ALIAS tpc_core)
set_target_properties(tpc_core PROPERTIES EXPORT_NAME Core)
target_compile_features(tpc_core INTERFACE cxx_std_23)
target_sources(
    tpc_core
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${TPC_INCLUDE_DIR}"
        FILES
            include/tpc/core/definitions/analytics_definitions.hpp
            include/tpc/core/definitions/client_definitions.hpp
            include/tpc/core/definitions/main_definitions.hpp
            include/tpc/core/logger/logger.hpp
)

add_library(tpc_analytics INTERFACE)
add_library(TPC::Analytics ALIAS tpc_analytics)
set_target_properties(tpc_analytics PROPERTIES EXPORT_NAME Analytics)
target_compile_features(tpc_analytics INTERFACE cxx_std_23)
target_sources(
    tpc_analytics
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${TPC_INCLUDE_DIR}"
        FILES
            include/tpc/analytics/analytics_manager/analytics_manager.hpp
            include/tpc/analytics/concepts/measurement_concept.hpp
            include/tpc/analytics/eigen_module.hpp
            include/tpc/analytics/models/basis_models.hpp
            include/tpc/analytics/models/three_dimension_models.hpp
            include/tpc/analytics/output/vtk_field_exporter.hpp
            include/tpc/analytics/svd/basis.hpp
            include/tpc/analytics/svd/svd_solver.hpp
)
target_link_libraries(
    tpc_analytics
    INTERFACE
        Eigen3::Eigen
        TPC::Core
        TPC::Utilities
)

add_library(
    tpc_system
    src/system/tpc.cpp
    src/system/client/client.cpp
    src/system/client/frame_receiver.cpp
    src/system/client/subscription.cpp
)
add_library(TPC::System ALIAS tpc_system)
set_target_properties(
    tpc_system
    PROPERTIES
        EXPORT_NAME System
        OUTPUT_NAME tpc_system
        CXX_EXTENSIONS OFF
)
target_compile_features(tpc_system PUBLIC cxx_std_23)
target_sources(
    tpc_system
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${TPC_INCLUDE_DIR}"
        FILES
            include/tpc/system/tpc.hpp
            include/tpc/system/client/client.hpp
            include/tpc/system/client/frame_receiver.hpp
            include/tpc/system/client/helpers/opcua_browse_adapter.hpp
            include/tpc/system/client/subscription.hpp
            include/tpc/system/models/data.hpp
)
target_link_libraries(
    tpc_system
    PUBLIC
        open62541pp::open62541pp
        STDEXEC::stdexec
        TPC::Analytics
        TPC::Core
        TPC::Utilities
)

add_library(tpc INTERFACE)
add_library(TPC::TPC ALIAS tpc)
set_target_properties(tpc PROPERTIES EXPORT_NAME TPC)
target_compile_features(tpc INTERFACE cxx_std_23)
target_sources(
    tpc
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS "${TPC_INCLUDE_DIR}"
        FILES include/tpc/tpc.hpp
)
target_link_libraries(tpc INTERFACE TPC::System)

if(TPC_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(tpc_system PRIVATE /W4 /permissive-)
    else()
        target_compile_options(tpc_system PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()

if(TPC_INSTALL)
    install(
        TARGETS tpc_utilities tpc_core tpc_analytics tpc_system tpc
        EXPORT TPCTargets
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/TPCConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        cmake/TPCConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/TPCConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TPC"
    )

    install(
        EXPORT TPCTargets
        FILE TPCTargets.cmake
        NAMESPACE TPC::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TPC"
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/TPCConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/TPCConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TPC"
    )
endif()
