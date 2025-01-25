
local MIDIEDITOR_RELEASE_VERSION_STRING = "3.9.0"
set_version(MIDIEDITOR_RELEASE_VERSION_STRING)
set_allowedplats("windows")

option("generate-repository", {
    description = "generate repositiory to update online repositories",
    default = false,
    values = {true, false},
    showmenu = true,
})

local installdir = "packaging/org.midieditor.midieditor/data/"
target("ProMidEdit") do
    set_languages("cxx17")
    add_rules("qt.widgetapp")

    -- Use system Qt6
    add_includedirs("$(env QTDIR)/include")
    add_linkdirs("$(env QTDIR)/lib")

    -- Add Qt6 multimedia include paths explicitly
    after_load(function (target)
        local qt_dir = os.getenv("QTDIR")
        if qt_dir then
            target:add("includedirs", path.join(qt_dir, "include/QtMultimedia"))
            target:add("includedirs", path.join(qt_dir, "include/QtMultimediaWidgets"))
        end
    end)

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
    if is_plat("windows") then
        add_defines("__WINDOWS_MM__")
        add_syslinks("winmm")
        add_files("midieditor.rc")
    end
    
    local bindir = path.join(installdir, "bin")
    local plugindir = path.join(bindir, "plugins")
    set_installdir(installdir)
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
    set_installdir("packaging/org.midieditor.manual/data/manual")
    add_installfiles("manual/(**)")
end

target("installer") do
    set_kind("phony")
    add_deps("ProMidEdit")
    
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
                    path.join(config.buildir(), "website", "ProMidEdit.exe")
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
    end
end
