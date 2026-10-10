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
            "-DSLINT_BUILD_EXAMPLES=OFF",
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
        if not os.isfile(program) or not os.isfile(runtime) then return end
        return {version = package:version_str(), includedirs = {path.join(root, "include", "slint")},
            linkdirs = {lib}, links = {package:is_plat("windows") and "slint_cpp.dll" or "slint_cpp"},
            rpathdirs = not package:is_plat("windows") and {lib, package:is_plat("macosx") and "@loader_path" or "$ORIGIN"} or nil,
            slint_compiler = program, slint_runtime = runtime}
    end)

    on_test(function (package)
        os.vrunv(package:installdir("bin", package:is_plat("windows") and "slint-compiler.exe" or "slint-compiler"), {"--version"})
        assert(package:check_cxxsnippets({sdk = '#include <slint.h>\nvoid test() { slint::SharedString text("Slint"); }'},
            {configs = {languages = "c++20"}}))
    end)
package_end()

rule("slint")
    after_load(function (target)
        assert(target:pkg("slint"), "slint requires add_packages(\"slint\")")
        local ui = assert(target:extraconf("rules", "slint", "ui"), "slint requires a UI entry file")
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
        local style = target:extraconf("rules", "slint", "style") or "fluent"
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
    after_build(function (target)
        local runtime = target:pkg("slint"):get("slint_runtime")
        if target:is_plat("macosx") then
            local bundle = path.join(target:targetdir(), target:basename() .. ".app", "Contents")
            local executable = path.join(bundle, "MacOS", target:basename())
            local frameworks = path.join(bundle, "Frameworks")
            os.rm(path.directory(bundle))
            os.mkdir(path.directory(executable), frameworks)
            os.cp(target:targetfile(), executable)
            local library = path.join(frameworks, "libslint_cpp.dylib")
            os.cp(runtime, library)
            os.vrunv("install_name_tool", {"-change", "@rpath/libslint_cpp.dylib",
                "@executable_path/../Frameworks/libslint_cpp.dylib", executable})
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
]], target:basename(), target:extraconf("rules", "slint", "bundle_identifier") or "org." .. target:name(),
                target:basename(), target:version() or "0.1.0", target:version() or "0.1.0"))
            os.vrunv("codesign", {"--force", "--sign", "-", library})
            os.vrunv("codesign", {"--force", "--sign", "-", path.directory(bundle)})
        else
            os.cp(runtime, target:targetdir())
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
        end
    end)
rule_end()
