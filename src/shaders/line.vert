#version 460

// Per-vertex attributes (line endpoints)
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;

// Uniform buffer with dynamic colors and settings
layout(std140, binding = 0) uniform UniformBuffer {
    mat4 mvpMatrix;
    vec2 screenSize;
    float time;
    float padding;
    vec4 backgroundColor;
    vec4 stripHighlightColor;
    vec4 stripNormalColor;
    vec4 rangeLineColor;
    vec4 borderColor;
    vec4 measureLineColor;
    vec4 playbackCursorColor;
    vec4 recordingIndicatorColor;
} ubo;

// Output to fragment shader
layout(location = 0) out vec4 fragColor;

void main() {
    // Convert from screen coordinates to normalized device coordinates
    vec2 ndc = (position / ubo.screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y; // Flip Y coordinate
    
    gl_Position = vec4(ndc, 0.0, 1.0);
    fragColor = color;
}
