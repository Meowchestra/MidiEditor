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
    // Use MVP matrix for consistent coordinate transformation
    gl_Position = ubo.mvpMatrix * vec4(position, 0.0, 1.0);
    fragColor = color;
}
