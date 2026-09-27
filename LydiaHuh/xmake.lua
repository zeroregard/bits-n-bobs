set_xmakever("3.0.0")

set_project("LydiaHuh")
set_version("1.0.0")
set_languages("cxx23")
set_warnings("allextra")

includes("lib/commonlibsse-ng")

add_rules("mode.debug", "mode.release")

if is_mode("release") then
    set_optimize("fastest")
    set_symbols("debug")
    set_runtimes("MT")
else
    set_runtimes("MTd")
end

add_defines("NOMINMAX")  -- CommonLib pulls in Windows.h

target("LydiaHuh")
    set_kind("shared")

    add_deps("commonlibsse-ng")
    add_rules("commonlibsse-ng.plugin", {
        name = "LydiaHuh",
        author = "zeroregard",
        description = "Lydia occasionally says 'Huh?'"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

    add_cxxflags("cl::/Zc:preprocessor", "cl::/utf-8", "cl::/wd4200", "cl::/wd4201")

    -- Copy the DLL into dist/ so dist/ is the complete, installable mod folder
    after_build(function (target)
        local plugins = path.join(os.projectdir(), "dist", "SKSE", "Plugins")
        os.mkdir(plugins)
        os.cp(target:targetfile(), plugins)
    end)
target_end()
