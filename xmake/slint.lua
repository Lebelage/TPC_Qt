-- Fetch and build the C++ SDK and UI compiler from the same Slint release.
package("slint")
    set_homepage("https://slint.dev")
    set_description("Slint C++ SDK built from source with Rust")
    add_urls("https://github.com/slint-ui/slint/archive/refs/tags/v$(version).tar.gz")
    add_versions("1.18.1", "fe485305ed303215e76c04918ee9aefbffbe229f18f979098ec36c7fa1dab28b")
    add_deps("cmake", "ninja")
    add_configs("shared", {description = "Build the shared Slint runtime", default = true, type = "boolean", readonly = true})

    on_install(function (package)
        import("lib.detect.find_tool")
        assert(find_tool("cargo") and find_tool("rustc"),
            "Building Slint requires Rust 1.92+ (cargo and rustc in PATH); install it with rustup")
        import("package.tools.cmake").install(package, {
            "-DBUILD_SHARED_LIBS=ON",
            "-DBUILD_TESTING=OFF",
            "-DSLINT_BUILD_TESTING=OFF",
            "-DSLINT_BUILD_EXAMPLES=OFF",
            "-DSLINT_BUILD_RUNTIME=ON",
            "-DSLINT_FEATURE_COMPILER=ON",
            "-DSLINT_COMPILER=",
            "-DSLINT_LIBRARY_CARGO_FLAGS=--locked",
            "-DSLINT_FEATURE_BACKEND_QT=OFF",
            "-DSLINT_FEATURE_RENDERER_SKIA=OFF"
        }, {cmake_generator = "Ninja"})
    end)

    on_fetch(function (package, opt)
        if opt.system then return end
        local root = package:installdir()
        local lib = path.join(root, "lib")
        local program = path.join(root, "bin", package:is_plat("windows") and "slint-compiler.exe" or "slint-compiler")
        local runtime = package:is_plat("windows") and path.join(lib, "slint_cpp.dll")
            or path.join(lib, package:is_plat("macosx") and "libslint_cpp.dylib" or "libslint_cpp.so")
        if package:is_plat("windows") and not os.isfile(runtime) then
            runtime = path.join(root, "bin", "slint_cpp.dll")
        end
        if not os.isfile(program) or not os.isfile(runtime) then return end
        return {version = package:version_str(), includedirs = {path.join(root, "include", "slint")},
            linkdirs = {lib}, links = {package:is_plat("windows") and "slint_cpp.dll" or "slint_cpp"},
            rpathdirs = not package:is_plat("windows") and {lib, package:is_plat("macosx") and "@loader_path" or "$ORIGIN"} or nil,
            slint_compiler = program, slint_runtime = runtime,
            slint_dlls = package:is_plat("windows") and table.join(
                os.files(path.join(root, "bin", "*.dll")), os.files(path.join(lib, "*.dll"))) or {}}
    end)

    on_test(function (package)
        os.vrunv(package:installdir("bin", package:is_plat("windows") and "slint-compiler.exe" or "slint-compiler"), {"--version"})
        assert(package:check_cxxsnippets({sdk = '#include <slint.h>\nvoid test() { slint::SharedString text("Slint"); }'},
            {configs = {languages = "c++20"}}))
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
            os.rm(path.directory(bundle))
            os.mkdir(path.directory(executable), frameworks)
            os.cp(target:targetfile(), executable)
            -- Relocate runtime dylibs recursively without modifying the cached SDK.
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
