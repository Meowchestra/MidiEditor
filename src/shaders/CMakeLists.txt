# Hardware Acceleration Shader Compilation for Qt RHI
#
# SHADER COMPILATION STRATEGY:
# 1. PRIMARY: Pre-compiled shaders from GitHub Actions (committed to repo)
# 2. FALLBACK: Local compilation for development (requires Qt dev tools)
#
# This CMakeLists.txt provides local compilation fallback with same parameters
# as GitHub Actions to ensure consistency.

# Check for pre-compiled shaders (primary method)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/compiled")
    message(STATUS "Using pre-compiled shaders from GitHub Actions")
    # Just install the pre-compiled shaders, no local compilation
    file(GLOB COMPILED_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/compiled/*.qsb")
    if(COMPILED_SHADERS)
        install(FILES ${COMPILED_SHADERS} DESTINATION shaders)
        message(STATUS "Pre-compiled shaders will be installed")
    endif()
    return()
endif()

# If no pre-compiled shaders, try to find qsb tool for local compilation
find_program(QSB_TOOL qsb)
if(NOT QSB_TOOL)
    message(WARNING "No pre-compiled shaders found and qsb tool not available.")
    message(WARNING "Hardware acceleration will use fallback rendering.")
    message(WARNING "Run GitHub Actions 'Compile Shaders' workflow to generate shaders.")
    return()
endif()

# Function to compile GLSL shaders to Qt RHI format
function(compile_shader SHADER_NAME)
    set(GLSL_VERT "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_NAME}.vert")
    set(GLSL_FRAG "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_NAME}.frag")
    set(QSB_VERT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.vert.qsb")
    set(QSB_FRAG "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.frag.qsb")
    
    # Compile vertex shader (modern desktop targets: GLSL 460 + HLSL 68)
    add_custom_command(
        OUTPUT ${QSB_VERT}
        COMMAND ${QSB_TOOL} --glsl 460 --hlsl 68 -o ${QSB_VERT} ${GLSL_VERT}
        DEPENDS ${GLSL_VERT}
        COMMENT "Compiling vertex shader: ${SHADER_NAME}.vert (GLSL 460 + HLSL 68)"
    )

    # Compile fragment shader (modern desktop targets: GLSL 460 + HLSL 68)
    add_custom_command(
        OUTPUT ${QSB_FRAG}
        COMMAND ${QSB_TOOL} --glsl 460 --hlsl 68 -o ${QSB_FRAG} ${GLSL_FRAG}
        DEPENDS ${GLSL_FRAG}
        COMMENT "Compiling fragment shader: ${SHADER_NAME}.frag (GLSL 460 + HLSL 68)"
    )
    
    # Add to shader target
    list(APPEND COMPILED_SHADERS ${QSB_VERT} ${QSB_FRAG})
    set(COMPILED_SHADERS ${COMPILED_SHADERS} PARENT_SCOPE)
endfunction()

# Compile all shaders
set(COMPILED_SHADERS "")
compile_shader(midi_event)
compile_shader(background)
compile_shader(line)
compile_shader(text)
compile_shader(piano)

# Create shader target
add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS})

# Install compiled shaders
install(FILES ${COMPILED_SHADERS} DESTINATION ${CMAKE_INSTALL_DATADIR}/midieditor/shaders)

message(STATUS "Hardware acceleration shaders will be compiled with qsb tool")
