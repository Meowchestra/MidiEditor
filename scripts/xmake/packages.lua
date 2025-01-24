
function add_all_requires()
    local qt_configs = {
        shared = true,
        vs_runtime = "MD"
    }
    
    add_requires("qt6base", {configs = qt_configs})
    add_requires("qt6widgets", {configs = qt_configs})
    add_requires("qtifw")
    add_requires("rtmidi")
end