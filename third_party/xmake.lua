target("open62541")
    set_kind("static")
    set_languages("c99")
    add_files("open62541/open62541.c")
    add_includedirs("open62541", {public = true})
    if is_plat("macosx") then
        add_defines("_DARWIN_C_SOURCE")
        if is_arch("arm64") then
            -- open62541 1.4 does not detect Apple ARM64's float representation.
            add_defines("UA_FLOAT_IEEE754=1", "UA_FLOAT_LITTLE_ENDIAN=1", {public = true})
        end
    elseif is_plat("windows") then
        add_syslinks("ws2_32", "iphlpapi", {public = true})
    elseif is_plat("linux") then
        add_syslinks("pthread", "m", "rt", {public = true})
    end
target_end()

target("open62541pp")
    set_kind("static")
    set_languages("c++23")
    add_files("open62541pp/src/**.cpp")
    add_includedirs("open62541pp/include", {public = true})
    add_deps("open62541", {public = true})
target_end()
