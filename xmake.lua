set_xmakever("3.0.0")
set_project("TPC_Slint")
set_version("0.1.0")
set_languages("c++23")
set_allowedplats("macosx", "windows", "linux")
set_config("builddir", "build")
set_targetdir("$(builddir)/$(plat)/$(arch)/$(mode)")
set_policy("package.requires_lock", true)
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

option("app", {default = true, showmenu = true, description = "Build the Slint desktop application"})

add_requires("eigen 5.0.1", "stdexec 2026.07.06", "nlohmann_json v3.12.0", {system = false})

if is_plat("windows") then
    add_cxxflags("/bigobj", "/Zc:__cplusplus", "/Zc:preprocessor", "/permissive-")
end
if is_mode("release") then
    set_optimize("smallest")
    set_strip("all")
    -- Homebrew Clang may emit newer bitcode than Apple's linker can read.
    set_policy("build.optimization.lto", not is_plat("macosx"))
end

includes("third_party")

-- Toolkit-independent libraries shared by the desktop application and tests.
target("tpc")
    set_kind("static")
    add_files("lib/tpc/src/**.cpp")
    add_includedirs("lib/tpc/include", {public = true})
    add_deps("open62541pp", {public = true})
    add_packages("eigen", "stdexec", {public = true})
target_end()

target("tpc_app")
    set_kind("static")
    add_files("src/application/*.cpp", "src/services/**.cpp", "src/viewmodels/*.cpp")
    add_includedirs("src", {public = true})
    add_deps("tpc", {public = true})
    add_packages("nlohmann_json", {public = true})
target_end()

if has_config("app") then
    local ui_style = "fluent-dark"
    includes("xmake/slint.lua")
    add_requires("slint 1.18.1", {system = false})
    target("TPC_Slint")
        set_kind("binary")
        add_deps("tpc_app")
        add_packages("slint")
        add_files("src/main.cpp", "src/ui/application_view_binding.cpp")
        add_rules("slint", {ui = "ui/app.slint", style = ui_style, bundle_identifier = "org.tpc.controller"})
        if is_plat("macosx") then
            add_files("src/ui/file_dialog_macos.mm")
            add_frameworks("AppKit", "UniformTypeIdentifiers")
        elseif is_plat("windows") then
            add_files("src/ui/file_dialog_windows.cpp")
            add_syslinks("ole32", "shell32", "uuid")
            add_ldflags("/subsystem:windows", {force = true})
        else
            add_files("src/ui/file_dialog_fallback.cpp")
        end
    target_end()

    target("tpc_field_map_ui_tests")
        set_kind("binary")
        set_default(false)
        add_deps("tpc_app")
        add_packages("slint")
        add_files("tests/field_map_ui_tests.cpp", "src/ui/application_view_binding.cpp", "src/ui/file_dialog_fallback.cpp")
        add_rules("slint", {ui = "ui/app.slint", style = ui_style})
        add_tests("interaction")
    target_end()
end

for name, source in pairs({
    tpc_scientific_tests = "tests/scientific_tests.cpp",
    tpc_logging_tests = "tests/logging_tests.cpp",
    tpc_sensor_configuration_tests = "tests/sensor_configuration_tests.cpp",
    tpc_test_frame = "lib/tpc/tests/test_frame.cpp"
}) do
    target(name)
        set_kind("binary")
        set_default(false)
        add_files(source)
        add_deps(name == "tpc_test_frame" and "tpc" or "tpc_app")
        if name == "tpc_scientific_tests" then
            add_defines('TPC_TEST_REFERENCE_PATH="' .. path.join(os.projectdir(), "tests/reference_map.json"):gsub("\\", "/") .. '"')
        end
        add_tests("validation")
    target_end()
end
