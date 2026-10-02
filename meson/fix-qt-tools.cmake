# Post-portfile hook for the vcpkg revision pinned in vcpkg.json. Fix tool
# relocation before dependent Qt ports invoke moc/rcc/qsb from this package.
find_program(TPC_QT_FIXUP_PYTHON NAMES python3 REQUIRED)
execute_process(
    COMMAND "${TPC_QT_FIXUP_PYTHON}"
        "${CMAKE_CURRENT_LIST_DIR}/../scripts/meson/fix-qt-tools-macos.py"
        "${CURRENT_PACKAGES_DIR}"
    RESULT_VARIABLE fixup_status
)
if(NOT fixup_status EQUAL 0)
    message(FATAL_ERROR "Could not repair relocated Qt tool rpaths")
endif()
