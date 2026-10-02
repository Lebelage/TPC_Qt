set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)

if(PORT MATCHES "^qt")
    # The generic Mach-O fixup flattens framework dependencies to @rpath/QtCore,
    # losing their .framework/Versions/A path. Qt already installs relocatable
    # framework IDs and tool rpaths; retain them.
    set(VCPKG_FIXUP_MACHO_RPATH OFF)
    list(APPEND Z_VCPKG_POST_PORTFILE_INCLUDES "${CMAKE_CURRENT_LIST_DIR}/../fix-qt-tools.cmake")
    list(APPEND VCPKG_HASH_ADDITIONAL_FILES
        "${CMAKE_CURRENT_LIST_DIR}/../fix-qt-tools.cmake"
        "${CMAKE_CURRENT_LIST_DIR}/../../scripts/meson/fix-qt-tools-macos.py")
    # Command Line Tools provide the SDK/compiler without a full Xcode bundle.
    # Keep Qt's SDK checks, but do not require an Xcode application version.
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS -DQT_NO_XCODE_MIN_VERSION_CHECK=ON)
endif()
