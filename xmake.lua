set_xmakever("3.0.0")
set_project("TPC_Slint")
set_version("0.1.0")
set_languages("c++23")
set_allowedplats("macosx", "windows", "linux")
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
    set_policy("build.optimization.lto", true)
end

includes("third_party")

-- Project libraries. SDK and UI dependencies belong only to the desktop target.
target("tpc")
    set_kind("static")
    add_files("lib/tpc/src/**.cpp")
    add_includedirs("lib/tpc/include", {public = true})
    add_deps("open62541pp", {public = true})
    add_packages("eigen", "stdexec", {public = true})
target_end()

target("tpc_app")
    set_kind("static")
    add_files("src/application/*.cpp", "src/services/**.cpp")
    add_includedirs("src", {public = true})
    add_deps("tpc", {public = true})
    add_packages("nlohmann_json", {public = true})
target_end()

if has_config("app") then
    includes("xmake/slint.lua")
    add_requires("slint >=1.18.0", {system = true})
    target("TPC_Slint")
        set_kind("binary")
        add_deps("tpc_app")
        add_packages("slint")
        add_files("src/main.cpp", "src/ui/application_view_binding.cpp", "src/viewmodels/*.cpp")
        add_rules("slint.cpp", {ui = "ui/app.slint", style = "fluent"})
        add_rules("slint.deploy", {bundle_identifier = "org.tpc.controller"})
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
end

for name, source in pairs({
    tpc_scientific_tests = "tests/scientific_tests.cpp",
    tpc_logging_tests = "tests/logging_tests.cpp",
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
