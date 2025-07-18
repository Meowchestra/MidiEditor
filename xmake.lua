
local MIDIEDITOR_RELEASE_VERSION_STRING = "3.8.1"
set_version(MIDIEDITOR_RELEASE_VERSION_STRING)

includes("scripts/xmake/packages.lua")
add_all_requires()

target("ProMidEdit") do
    add_packages({
        "rtmidi",
        "qt5widgets"
    })
    add_rules("qt.widgetapp")
    add_frameworks({
        "QtGui",
        "QtWidgets",
        "QtCore",
        "QtNetwork",
        "QtXml",
        "QtMultimedia",
        "QtMultimediaWidgets"
    })
    add_files("src/**.cpp")
    add_files("src/**.h")
    add_files("resources.qrc")
    if is_arch("x86_64") then
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
    elseif is_plat("windows", "mingw") then
        add_defines("__WINDOWS_MM__")
        add_syslinks("winmm")
        add_files("midieditor.rc")
    elseif is_plat("macosx") then
        add_defines("__MACOSX_CORE__")
        add_frameworks("CoreMidi", "CoreAudio", "CoreFoundation")
        -- TODO: icons
        add_installfiles("midieditor.icns")
    end
    
    local installdir = "packaging/org.midieditor.midieditor/data/"
    local bindir = path.join(installdir, "bin")
    local plugindir = path.join(bindir, "plugins")
    set_installdir(installdir)

    -- Install metronome files
    add_installfiles("run_environment/metronome/metronome-01.wav", {prefixdir = "metronome"})
    if is_plat("windows", "mingw") then
        set_values("qt.deploy.flags", {
            "--plugindir", plugindir,
            "--libdir", bindir
        })
        after_install(function (target)
            print("after_install of target ProMidEdit")
            import("core.base.option")
            import("core.project.config")
            import("lib.detect.find_tool")

            -- get windeployqt
            local windeployqt_tool = assert(
                find_tool("windeployqt", {check = "--help"}),
                "windeployqt.exe not found!")
            local windeployqt = windeployqt_tool.program

            -- deploy necessary dll with size optimizations
            local deploy_argv = {
                "--compiler-runtime",
                "--release",
                "--no-opengl-sw",           -- Remove opengl32sw.dll
                "--no-system-d3d-compiler", -- Remove D3Dcompiler_47.dll
                "--no-translations"         -- Remove Qt translations
            }
            if option.get("diagnosis") then
                table.insert(deploy_argv, "--verbose=2")
            elseif option.get("verbose") then
                table.insert(deploy_argv, "--verbose=1")
            else
                table.insert(deploy_argv, "--verbose=0")
            end
            local bindir = path.join(target:installdir(), "bin")
            local plugindir = path.join(bindir, "plugins")
            print("Deploying Qt to:", bindir)
            table.join2(deploy_argv, {"--plugindir", plugindir})
            table.join2(deploy_argv, {"--libdir", bindir})
            table.insert(deploy_argv, path.join(bindir, "ProMidEdit.exe"))
            print("Running windeployqt with args:", table.concat(deploy_argv, " "))
            os.iorunv(windeployqt, deploy_argv)
            os.rm(path.join(bindir, "**", "dsengine.dll"))
        end)
    end
end

target("installer") do
    set_kind("phony")

    set_installdir("packaging/org.midieditor.manual/data/manual")
    add_installfiles("manual/(**)")
    add_packages("qtifw")
    add_deps("ProMidEdit")

    before_install(function (target, opt)
        -- Ensure ProMidEdit is installed first (since dependencies don't auto-install)
        import("core.project.task")
        print("Installing ProMidEdit dependency...")
        task.run("install", {target = "ProMidEdit"})
    end)

    after_install(function (target, opt)
        if is_plat("windows") then
            print("generate off-line installer")
            import("core.project.config")
            import("lib.detect.find_tool")
            local qtifw_dir = target:pkg("qtifw"):installdir()
            local binarycreator_path = path.join(qtifw_dir, "/bin/binarycreator.exe")
            -- generate windows package
            local buildir = config.buildir()
            local package_argv = {
                "--config", "scripts/packaging/windows/config.xml",
                "--packages", "packaging",
                "packaging/Install.exe"
            }
            print("Running binarycreator with args:", table.concat(package_argv, " "))
            os.iorunv(binarycreator_path, package_argv)
        end
    end)
end
