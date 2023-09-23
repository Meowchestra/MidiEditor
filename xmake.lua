
local MIDIEDITOR_RELEASE_VERSION_STRING = "3.4.1"
set_version(MIDIEDITOR_RELEASE_VERSION_STRING)

includes("scripts/xmake/packages.lua")
add_all_requires()

target("translation") do
    set_kind("phony")
    add_packages("qt5widgets")
    on_build(function (target)
        print("compiling translation file")
        import("core.project.config")
        import("lib.detect.find_tool")

        -- get lrelease
        local lrelease_tool = assert(
            find_tool("lrelease", {check = "-help"}),
            "lrelease.exe not found!")
        local lrelease = lrelease_tool.program

        local translation_declare = {}
        for _, filepath in ipairs(os.files("locale/*.ts")) do
            table.insert(translation_declare, filepath)
        end
        os.iorunv(lrelease, translation_declare)
    end)
end

target("ProMidEdit") do
    add_deps("translation")
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
    if is_plat("windows") then
        set_values("qt.deploy.flags", {
            "--plugindir", plugindir,
            "--libdir", bindir
        })
        after_install(function (target) 
            os.rm(path.join(bindir, "**", "dsengine.dll"))
        end)
    elseif is_plat("mingw") then
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
            
            -- deploy necessary dll
            -- mingw with posix thread should be used, or dll error will be reported
            local deploy_argv = {"--compiler-runtime", "--release"}
            if option.get("diagnosis") then
                table.insert(deploy_argv, "--verbose=2")
            elseif option.get("verbose") then
                table.insert(deploy_argv, "--verbose=1")
            else
                table.insert(deploy_argv, "--verbose=0")
            end
            local bindir = path.join(target:installdir(), "bin")
            local plugindir = path.join(bindir, "plugins")
            -- print(plugindir)
            table.join2(deploy_argv, {"--plugindir", plugindir})
            table.join2(deploy_argv, {"--libdir", bindir})
            table.insert(deploy_argv, bindir)
            os.iorunv(windeployqt, deploy_argv)
            os.rm(path.join(bindir, "**", "dsengine.dll"))
        end)
    end
end

target("installer") do
    set_kind("phony")
    
    local installdir = 
    set_installdir("packaging/org.midieditor.manual/data/manual")
    add_installfiles("manual/(**)")
    add_packages("qtifw")
    add_deps("ProMidEdit")

    after_install(function (target, opt)
        if is_plat("windows", "mingw") then
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
            os.iorunv(binarycreator_path, package_argv)
        end
    end)
end
