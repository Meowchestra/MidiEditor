
local MIDIEDITOR_RELEASE_VERSION_STRING = "4.1.2"
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

    add_packages({
        "qt6widgets"
    })
    add_rules("qt.widgetapp")
    add_frameworks({
        "QtGui",
        "QtWidgets",
        "QtCore",
        "QtNetwork",
        "QtXml",
        "QtMultimedia",
        "QtOpenGL",
        "QtOpenGLWidgets"
    })

    -- Link system OpenGL libraries for cross-platform compatibility
    if is_plat("windows") then
        add_syslinks("opengl32")
    elseif is_plat("linux") then
        add_syslinks("GL", "GLU")
    elseif is_plat("macosx") then
        add_frameworks("OpenGL")
    end

    -- Add source files, including only the main rtmidi files (not examples/tests)
    add_files("src/*.cpp")
    add_files("src/MidiEvent/**.cpp")
    add_files("src/gui/**.cpp")
    add_files("src/midi/*.cpp")
    add_files("src/protocol/**.cpp")
    add_files("src/tool/**.cpp")
    -- Add only the main rtmidi source files
    add_files("src/midi/rtmidi/RtMidi.cpp")

    add_files("src/*.h")
    add_files("src/MidiEvent/**.h")
    add_files("src/gui/**.h")
    add_files("src/midi/*.h")
    add_files("src/protocol/**.h")
    add_files("src/tool/**.h")
    -- Add only the main rtmidi header files
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
        add_files("midieditor.rc")
        -- Set Windows subsystem to GUI to prevent console window
        set_kind("binary")
        -- Use MSVC-style linker flags for Windows
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
	
    add_installfiles("run_environment/metronome/metronome-01.wav", {prefixdir = "metronome"})
    if is_plat("windows") then
        local configs = {
            "--plugindir", plugindir,
            "--libdir", bindir
        }
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
                print("generate site")
                print("  generate repository")
                local repo_argv = {
                    "--update-new-components",
                    "--packages", "packaging",
                    path.join(config.buildir(), "website", "repository")
                }
                os.iorunv(repogen_path, repo_argv)
                print("  generate installer")
                local package_argv = {
                    "--config", "scripts/packaging/windows/config.xml",
                    "--packages", "packaging",
                    path.join(config.buildir(), "website", "MidiEditor.exe")
                }
                os.iorunv(binarycreator_path, package_argv)
                print("  copy online manual")
                os.cp("manual/*", path.join(config.buildir(), "website"))
            else
                print("generate off-line installer")
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
                DEPENDS = "libc6(>=2.19), libfluidsynth3, qtbase6-dev, qtdeclarative6-dev, libqt6webkit6-dev, libsqlite3-dev, qt6-default, qtmultimedia6-dev, libqt6multimedia6, qttools6-dev-tools, libqt6multimedia6-plugins, libasound2, libgstreamer1.0-0, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-plugins-ugly, gstreamer1.0-libav, gstreamer1.0-doc, gstreamer1.0-tools",
                SIZE = 70, -- in kb, todo
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
            os.runv("chmod", {
                "+x", path.join(target:installdir(), "bin", target:deps()["MidiEditor"]:filename())})
            os.runv("fakeroot", {"dpkg-deb", "--build", target:installdir()})
        end)
    end
end