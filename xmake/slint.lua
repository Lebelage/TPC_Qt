-- Installed Slint C++ SDK: xmake owns discovery/linkage; this rule owns UI generation.
package("slint")
    set_homepage("https://slint.dev")
    set_description("Installed Slint C++ SDK")
    on_fetch(function (package, opt)
        if not opt.system then return false end
        import("lib.detect.find_tool")
        import("core.base.semver")
        local compiler = find_tool("slint-compiler", {force = true})
        local program = compiler and compiler.program
        assert(program and os.isfile(program),
            "slint-compiler is required in PATH (Homebrew slint-cpp provides only the library; install the compiler separately)")
        while os.islink(program) do
            program = path.absolute(os.readlink(program), path.directory(program))
        end
        local root = path.directory(path.directory(program))
        -- Homebrew ships the C++ library without the compiler; discover it separately.
        if package:is_plat("macosx") and not os.isfile(path.join(root, "include", "slint", "slint.h")) then
            local brew = find_tool("brew")
            assert(brew, "Slint headers not found beside slint-compiler; install the C++ SDK")
            root = os.iorunv(brew.program, {"--prefix", "slint-cpp"}):trim()
        end
        local include = path.join(root, "include", "slint")
        local lib = path.join(root, "lib")
        local runtime = package:is_plat("windows") and path.join(root, "bin", "slint_cpp.dll")
            or path.join(lib, package:is_plat("macosx") and "libslint_cpp.dylib" or "libslint_cpp.so")
        if package:is_plat("windows") and not os.isfile(runtime) then
            runtime = path.join(lib, "slint_cpp.dll")
        end
        assert(os.isfile(path.join(include, "slint.h")), "Incomplete Slint SDK: missing %s/slint.h", include)
        assert(os.isfile(runtime), "Slint SDK runtime is missing: %s", runtime)
        if package:is_plat("windows") then
            assert(os.isfile(path.join(lib, "slint_cpp.lib")), "Slint SDK must include lib/slint_cpp.lib for MSVC")
        end
        local version = os.iorunv(program, {"--version"}):match("(%d+%.%d+%.%d+)")
        assert(version and semver.satisfies(version, ">=1.18.0"), "Slint C++ SDK 1.18 or newer is required")
        local config = path.join(lib, "cmake", "Slint", "SlintConfigVersion.cmake")
        if os.isfile(config) then
            local sdk_version = io.readfile(config):match('set%(PACKAGE_VERSION "([%d%.]+)"%)')
            assert(not sdk_version or sdk_version == version,
                "Slint version mismatch: compiler %s, C++ SDK %s; install matching versions", version, sdk_version)
        end
        return {version = version, includedirs = {include}, linkdirs = {lib}, links = {"slint_cpp"},
            rpathdirs = not package:is_plat("windows") and {lib, "@loader_path"} or nil,
            slint_compiler = program, slint_runtime = runtime,
            slint_dlls = package:is_plat("windows") and table.join(
                os.files(path.join(root, "bin", "*.dll")), os.files(path.join(lib, "*.dll"))) or {}}
    end)
package_end()

rule("slint.cpp")
    after_load(function (target)
        assert(target:pkg("slint"), "slint.cpp requires add_packages(\"slint\")")
        local ui = assert(target:extraconf("rules", "slint.cpp", "ui"), "slint.cpp requires a UI entry file")
        local generated = path.absolute(path.join(target:autogendir(), "slint"), os.projectdir())
        target:data_set("slint.generated", generated)
        target:data_set("slint.ui", path.absolute(ui, os.projectdir()))
        target:add("includedirs", generated)
        target:add("files", path.join(generated, path.basename(ui) .. ".cpp"), {always_added = true})
    end)
    before_build(function (target)
        import("core.project.depend")
        local generated = target:data("slint.generated")
        local ui = target:data("slint.ui")
        local header = path.join(generated, path.basename(ui) .. ".h")
        local source = path.join(generated, path.basename(ui) .. ".cpp")
        local compiler = target:pkg("slint"):get("slint_compiler")
        local style = target:extraconf("rules", "slint.cpp", "style") or "fluent"
        local inputs = os.files(path.join(path.directory(ui), "**"))
        table.sort(inputs)
        table.insert(inputs, compiler)
        depend.on_changed(function ()
            os.mkdir(generated)
            os.vrunv(compiler, {ui, "-f", "cpp", "-o", header,
                "--cpp-file=" .. source, "--style", style, "--embed-resources=embed-files"})
        end, {dependfile = target:dependfile(header), files = inputs,
            values = {inputs, header, source, style, "embed-files"},
            changed = target:is_rebuilt() or not os.isfile(header) or not os.isfile(source)})
    end)
rule_end()

-- Keep SDK runtime deployment with the SDK integration, not in the application target.
rule("slint.deploy")
    after_build(function (target)
        local runtime = target:pkg("slint"):get("slint_runtime")
        if target:is_plat("macosx") then
            import("utils.binary.rpath", {alias = "rpath_utils"})
            local bundle = path.join(target:targetdir(), target:basename() .. ".app", "Contents")
            local executable = path.join(bundle, "MacOS", target:basename())
            local frameworks = path.join(bundle, "Frameworks")
            os.mkdir(path.directory(executable), frameworks)
            os.cp(target:targetfile(), executable)
            -- Relocate SDK/Homebrew dylibs recursively; never modify the installed SDK.
            local copied = {}
            local function relocate(source, destination)
                local dependencies = os.iorunv("otool", {"-L", source})
                for dependency in dependencies:gmatch("\n%s+([^\n]+) %(compatibility") do
                    local name = path.filename(dependency)
                    if name ~= path.filename(source) and not dependency:startswith("/usr/lib/")
                        and not dependency:startswith("/System/Library/") then
                        local resolved = dependency:gsub("@loader_path", path.directory(source))
                        if dependency:startswith("@rpath/") then
                            local suffix = dependency:sub(8)
                            for _, dir in ipairs(rpath_utils.list(source) or {}) do
                                dir = dir:gsub("@loader_path", path.directory(source))
                                local candidate = path.join(dir, suffix)
                                if os.isfile(candidate) then resolved = candidate; break end
                            end
                        end
                        if name == "libslint_cpp.dylib" then resolved = runtime end
                        assert(os.isfile(resolved), "Cannot resolve SDK runtime dependency: %s", dependency)
                        assert(name:endswith(".dylib"), "SDK framework dependency needs bundling support: %s", dependency)
                        local library = path.join(frameworks, name)
                        if not copied[name] then
                            copied[name] = resolved
                            os.cp(resolved, library)
                            relocate(resolved, library)
                            os.vrunv("install_name_tool", {"-id", "@rpath/" .. name, library})
                        else
                            assert(copied[name] == resolved, "Conflicting runtime libraries: %s", name)
                        end
                        os.vrunv("install_name_tool", {"-change", dependency,
                            "@executable_path/../Frameworks/" .. name, destination})
                    end
                end
            end
            relocate(target:targetfile(), executable)
            io.writefile(path.join(bundle, "Info.plist"), string.format([[<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>%s</string>
<key>CFBundleIdentifier</key><string>%s</string>
<key>CFBundleName</key><string>%s</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>%s</string>
<key>CFBundleShortVersionString</key><string>%s</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
]], target:basename(), target:extraconf("rules", "slint.deploy", "bundle_identifier") or "org." .. target:name(),
                target:basename(), target:version() or "0.1.0", target:version() or "0.1.0"))
            for name in pairs(copied) do
                os.vrunv("codesign", {"--force", "--sign", "-", path.join(frameworks, name)})
            end
            os.vrunv("codesign", {"--force", "--sign", "-", path.directory(bundle)})
        else
            os.cp(runtime, target:targetdir())
            for _, dll in ipairs(table.wrap(target:pkg("slint"):get("slint_dlls"))) do os.cp(dll, target:targetdir()) end
        end
    end)
    on_install(function (target)
        if target:is_plat("macosx") then
            os.mkdir(target:installdir())
            os.cp(path.join(target:targetdir(), target:basename() .. ".app"), target:installdir())
        else
            local bin = path.join(target:installdir(), "bin")
            os.mkdir(bin)
            os.cp(target:targetfile(), bin)
            os.cp(target:pkg("slint"):get("slint_runtime"), bin)
            for _, dll in ipairs(table.wrap(target:pkg("slint"):get("slint_dlls"))) do os.cp(dll, bin) end
        end
    end)
rule_end()
