
local MIDIEDITOR_RELEASE_VERSION_STRING = "4.4.1"
add_requires("zlib")
add_requires("uchardet")
set_version(MIDIEDITOR_RELEASE_VERSION_STRING)
set_allowedplats("windows", "linux", "macosx")

option("generate-repository", {
    description = "generate repositiory to update online repositories",
    default = false,
    values = {true, false},
    showmenu = true,
})
option("libraries-from-apt", {
    description = "use libraries from apt rather than xmake-repo",
    default = false,
    values = {true, false},
    showmenu = is_host("linux") and (linuxos.name() == "ubuntu"),
})
option("fluidsynth_dir", {
    description = "path to pre-built FluidSynth directory",
    default = "",
    showmenu = true,
})

target("MidiEditor") do
    set_languages("cxxlatest")
    set_targetdir("bin")

    -- Performance optimizations (MIDI-safe: no -ffast-math due to timing precision requirements)
    if is_mode("release") then
        -- Use xmake's built-in optimization and add additional flags
        set_optimize("fastest")  -- Sets -O3 for GCC/Clang, /Ox for MSVC
        add_cxxflags("-DNDEBUG")
        set_strip("all")  -- Strip debug symbols in release

        -- MSVC specific optimizations (Windows platform)
        if is_plat("windows") then
            -- Explicitly override with maximum optimization (in case set_optimize doesn't work)
            add_cxxflags("/Ox")  -- Maximum optimization (override any -O2)
            add_cxxflags("/Ob2", "/Oi", "/Ot", "/GL")  -- Inline expansion, intrinsics, favor speed, whole program optimization
            add_cxxflags("/arch:AVX2")  -- x86-64-v3 compatibility

            -- Advanced MSVC optimizations
            add_cxxflags("/Gy")  -- Function-level linking
            add_cxxflags("/Gw")  -- Global data optimization
            add_cxxflags("/GA")  -- Optimize for Windows applications
            -- No /favor flag for optimal performance on both Intel and AMD

            add_ldflags("/LTCG", "/OPT:REF", "/OPT:ICF")  -- Link-time code generation and optimization
        end

        -- GCC/Clang specific optimizations (Linux/macOS)
        if is_plat("linux", "macosx") then
            -- -O3 is already set by set_optimize("fastest"), add additional optimizations
            add_cxxflags("-fomit-frame-pointer")
            add_cxxflags("-march=x86-64-v3", "-mtune=generic")
            add_cxxflags("-funroll-loops", "-fprefetch-loop-arrays")

            -- Advanced optimization flags
            add_cxxflags("-fvectorize", "-fslp-vectorize")  -- Enhanced vectorization
            add_cxxflags("-ffunction-sections", "-fdata-sections")  -- Dead code elimination
            add_cxxflags("-falign-functions=32", "-falign-loops=32")  -- Cache-friendly alignment
            add_cxxflags("-fmerge-all-constants")  -- Merge identical constants
            add_cxxflags("-fmodulo-sched", "-fmodulo-sched-allow-regmoves")  -- Loop scheduling

            -- Safe floating-point optimizations (no -ffast-math)
            add_cxxflags("-fno-math-errno", "-ffinite-math-only")
            add_cxxflags("-fno-signed-zeros", "-fno-trapping-math")  -- Additional safe FP opts

            -- Link-time optimization with advanced flags
            add_cxxflags("-flto=thin")  -- Thin LTO for faster builds
            add_ldflags("-flto=thin", "-Wl,-O3", "-Wl,--as-needed")
            add_ldflags("-Wl,--gc-sections", "-Wl,--icf=all")  -- Aggressive dead code removal
        end
    elseif is_mode("debug") then
        set_optimize("none")
        add_cxxflags("-g")
        set_symbols("debug")
    end

    add_packages("qt6widgets", "zlib", "uchardet")
    add_rules("qt.widgetapp")
    add_frameworks({
        "QtGui", "QtWidgets", "QtCore", "QtNetwork",
        "QtOpenGL", "QtOpenGLWidgets", "QtConcurrent"
    })

    -- Link system OpenGL libraries for cross-platform compatibility
    if is_plat("windows") then
        add_syslinks("opengl32")
    elseif is_plat("linux") then
        add_syslinks("GL", "GLU")
    elseif is_plat("macosx") then
        add_frameworks("OpenGL")
    end

    -- Optional FluidSynth support for built-in synthesizer
    on_load(function (target)
        import("lib.detect.find_library")
        import("lib.detect.find_path")

        local fs_dir = get_config("fluidsynth_dir")
        local inc_dirs = {"/usr/include", "/usr/local/include", "/opt/homebrew/include"}
        local lib_dirs = {}

        -- If a pre-built dir was provided, search there first
        if fs_dir and fs_dir ~= "" then
            table.insert(inc_dirs, 1, path.join(fs_dir, "include"))
            table.insert(lib_dirs, path.join(fs_dir, "lib"))
        end

        -- On Windows, try different variations of the FluidSynth library name
        local lib_names = is_plat("windows") and {"fluidsynth", "libfluidsynth", "libfluidsynth-3"} or {"fluidsynth"}
        -- Check explicit directory without find_library tests if provided
        if fs_dir and fs_dir ~= "" then
            for _, name in ipairs(lib_names) do
                local ext = is_plat("windows") and ".lib" or (is_plat("macosx") and ".dylib" or ".so")
                local p = path.join(fs_dir, "lib", name .. ext)
                if os.isfile(p) then
                    fs_lib = p
                    fs_libname = name
                    break
                end
            end
        end
        
        -- Fallback to system search
        if not fs_lib then
            for _, name in ipairs(lib_names) do
                fs_lib = find_library(name, {paths = lib_dirs})
                if fs_lib then
                    fs_libname = name
                    break
                end
            end
        end
        local fs_inc = find_path("fluidsynth.h", inc_dirs)

        if fs_lib then
            target:add("defines", "FLUIDSYNTH_SUPPORT")
            target:add("links", fs_libname)
            if #lib_dirs > 0 then
                target:add("linkdirs", lib_dirs)
            end
            if fs_inc then
                target:add("includedirs", fs_inc)
            end
            print("FluidSynth found - enabling built-in synthesizer support")
        else
            print("FluidSynth not found - built-in synthesizer disabled")
        end
    end)

    add_files("src/*.cpp")
    add_files("src/converter/**.cpp")
    add_files("src/MidiEvent/**.cpp")
    add_files("src/gui/**.cpp")
    add_files("src/midi/*.cpp")
    add_files("src/protocol/**.cpp")
    add_files("src/support/**.cpp")
    add_files("src/tool/**.cpp")
    add_files("src/midi/rtmidi/RtMidi.cpp")

    add_files("src/*.h")
    add_files("src/converter/**.h")
    add_files("src/MidiEvent/**.h")
    add_files("src/gui/**.h")
    add_files("src/midi/*.h")
    add_files("src/protocol/**.h")
    add_files("src/support/**.h")
    add_files("src/tool/**.h")
    add_files("src/midi/rtmidi/RtMidi.h")
    add_files("resources.qrc")

    if is_arch("x64", "x86_64") then
        add_defines("__ARCH64__")
    end
    add_defines("MIDIEDITOR_RELEASE_VERSION_ID_DEF=" .. 0)
    add_defines("MIDIEDITOR_RELEASE_DATE_DEF=" .. os.date("%x"))
    add_defines("MIDIEDITOR_RELEASE_VERSION_STRING_DEF=" .. MIDIEDITOR_RELEASE_VERSION_STRING)
    if is_plat("linux", "bsd") then
        add_defines({
            "__LINUX_ALSASEQ__",
            "__LINUX_ALSA__"
        })
        add_syslinks("asound")
    elseif is_plat("windows") then
        add_defines("__WINDOWS_MM__")
        add_syslinks("winmm")
        add_syslinks("dwmapi")
        add_files("midieditor.rc")
        set_kind("binary")
        add_ldflags("/SUBSYSTEM:WINDOWS", {force = true})
    elseif is_plat("macosx") then
        add_defines("__MACOSX_CORE__")
        add_frameworks("CoreMidi", "CoreAudio", "CoreFoundation")
        add_installfiles("midieditor.icns")
    end
    
    local installdir = "packaging/org.midieditor.midieditor/data/"
    local bindir = path.join(installdir, "bin")
    local plugindir = path.join(bindir, "plugins")
    set_installdir(installdir)

    if is_plat("windows") then
        local configs = {"--release", "--no-translations", "--no-opengl-sw", "--no-system-d3d-compiler", "--no-compiler-runtime", "--skip-plugin-types", "qmltooling,sqldrivers,positioning,sensors,serialport,texttospeech,webengine", "--plugindir", plugindir, "--libdir", bindir}
        set_values("qt.deploy.flags", configs)
        after_install(function (target) 
            os.rm(path.join(bindir, "**", "dsengine.dll"))
        end)
        after_uninstall(function (target)
            os.rm(path.join(installdir, "**", "*.dll"))
            os.rm(path.join(installdir, "**", "*.exe"))
        end)
    end
end

target("manual") do
    set_kind("phony")
    set_enabled(is_plat("windows"))
    set_installdir("packaging/org.midieditor.manual/data/manual")
    add_installfiles("manual/(**)")
end

target("installer") do
    set_kind("phony")
    set_enabled(is_plat("windows", "linux"))
    add_deps("MidiEditor")

    set_installdir(installdir)
    if is_plat("windows") then
        add_deps("manual")
        add_packages("qtifw")
        after_install(function (target, opt)
            import("core.project.config")
            local qtifw_dir = target:pkg("qtifw"):installdir()
            local binarycreator_path = path.join(qtifw_dir, "/bin/binarycreator.exe")
            local repogen_path = path.join(qtifw_dir, "/bin/repogen.exe")
            if config.get("generate-repository") then
                local repo_argv = {
                    "--update-new-components",
                    "--packages", "packaging",
                    path.join(config.buildir(), "website", "repository")
                }
                os.iorunv(repogen_path, repo_argv)
                local package_argv = {
                    "--config", "scripts/packaging/windows/config.xml",
                    "--packages", "packaging",
                    path.join(config.buildir(), "website", "MidiEditor.exe")
                }
                os.iorunv(binarycreator_path, package_argv)
                os.cp("manual/*", path.join(config.buildir(), "website"))
            else
                local package_argv = {
                    "--config", "scripts/packaging/windows/config.xml",
                    "--packages", "packaging",
                    "packaging/Install.exe"
                }
                os.iorunv(binarycreator_path, package_argv)
            end
        end)
    elseif is_plat("linux") and linuxos.name() == "ubuntu" then
        add_installfiles("scripts/packaging/debian/MidiEditor.desktop", {prefixdir = "usr/share/applications"})
        add_installfiles("scripts/packaging/debian/logo48.png", {prefixdir = "usr/share/pixmaps"})
        add_installfiles("scripts/packaging/debian/copyright", {prefixdir = "usr/share/doc/midieditor/copyright"})
        add_installfiles("$(buildir)/control", {prefixdir = "DEBIAN"})
        add_configfiles("scripts/packaging/debian/control", {
            pattern = "{(.-)}",
            variables = {
                PACKAGE = 1,
                DEPENDS = "libc6(>=2.19), libfluidsynth3, qtbase6-dev, qtdeclarative6-dev, libqt6webkit6-dev, libsqlite3-dev, qt6-default, qttools6-dev-tools, libasound2, libgstreamer1.0-0, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-plugins-ugly, gstreamer1.0-libav, gstreamer1.0-doc, gstreamer1.0-tools",
                SIZE = 70,
            }
        })
        after_install(function (target, opt)
            import("core.project.config")
            local installdir_glob = path.join(target:installdir(), "**")
            for _, file in ipairs(os.dirs(installdir_glob)) do
                os.runv("chmod", {"755", file})
            end
            for _, file in ipairs(os.files(installdir_glob)) do
                os.runv("chmod", {"644", file})
            end
            os.runv("chmod", {"+x", path.join(target:installdir(), "bin", target:deps()["MidiEditor"]:filename())})
            os.runv("fakeroot", {"dpkg-deb", "--build", target:installdir()})
        end)
    end
end