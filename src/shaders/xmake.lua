-- Shader integration for Qt RHI hardware acceleration
-- Primary: Uses pre-compiled .qsb files from GitHub Actions
-- Fallback: Local compilation if qsb tool is available

-- Check if compiled shaders exist
local compiled_dir = path.join(os.scriptdir(), "compiled")
local has_compiled_shaders = os.isdir(compiled_dir)

-- Check for local qsb tool as optional fallback
local qsb_tool = "qsb"
local can_compile_locally = false

-- Try to detect qsb tool
if os.host() == "windows" then
    local result = os.iorunv("where", {"qsb"})
    can_compile_locally = result and result:trim() ~= ""
else
    local result = os.iorunv("which", {"qsb"})
    can_compile_locally = result and result:trim() ~= ""
end

if not has_compiled_shaders then
    if can_compile_locally then
        print("Info: No pre-compiled shaders found, will compile locally with qsb")
    else
        print("Warning: No compiled shaders found and qsb tool not available")
        print("Hardware acceleration will use fallback rendering.")
        print("Run GitHub Actions 'Compile Shaders' workflow to generate shaders")
    end
end

-- Local shader compilation rule (fallback when pre-compiled shaders not available)
if can_compile_locally then
    rule("compile_shader_local")
        set_extensions(".vert", ".frag")
        on_build_file(function (target, sourcefile, opt)
            local outputdir = path.join(target:targetdir(), "shaders", "compiled")
            os.mkdir(outputdir)

            local basename = path.basename(sourcefile)
            local outputfile = path.join(outputdir, basename .. ".qsb")

            -- Use same parameters as GitHub Actions: GLSL 460 + HLSL 68
            local argv = {
                "--glsl", "460",
                "--hlsl", "68", 
                "-o", outputfile,
                sourcefile
            }

            print("Locally compiling shader: " .. basename .. " (GLSL 460 + HLSL 68)")
            os.vrunv(qsb_tool, argv)
            
            if os.isfile(outputfile) then
                print("✓ Successfully compiled: " .. basename .. " -> " .. basename .. ".qsb")
            else
                raise("✗ Failed to compile shader: " .. basename)
            end
        end)
end

-- Shader resource target (pre-compiled preferred, local compilation fallback)
target("midieditor_shaders")
    set_kind("object")

    -- Add local compilation if needed and available
    if not has_compiled_shaders and can_compile_locally then
        add_rules("compile_shader_local")
        add_files("*.vert", "*.frag")
    end

    -- Install shaders
    after_build(function (target)
        local installdir = path.join("$(buildir)", "shaders")
        os.mkdir(installdir)

        -- Copy pre-compiled shaders if they exist
        if has_compiled_shaders then
            os.cp(path.join(compiled_dir, "*.qsb"), installdir)
            print("Pre-compiled shaders installed to: " .. installdir)
        -- Copy locally compiled shaders
        elseif can_compile_locally then
            local local_compiled = path.join(target:targetdir(), "shaders", "compiled")
            if os.isdir(local_compiled) then
                os.cp(path.join(local_compiled, "*.qsb"), installdir)
                print("Locally compiled shaders installed to: " .. installdir)
            end
        else
            print("No shaders available - hardware acceleration will use fallback rendering")
            print("Run GitHub Actions 'Compile Shaders' workflow to generate shaders")
        end

        -- Copy resource file if it exists
        local qrc_file = path.join(os.scriptdir(), "shaders.qrc")
        if os.isfile(qrc_file) then
            os.cp(qrc_file, installdir)
            print("Shader resource file installed")
        end
    end)

-- Add to main target dependencies if hardware acceleration is enabled
if has_config("hardware_acceleration") then
    add_deps("midieditor_shaders")
end
