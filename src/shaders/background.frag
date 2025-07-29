#version 460

// Input from vertex shader
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 screenPos;

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

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Render solid background exactly like software QPainter::fillRect()
    // Uses dynamic color from Appearance::backgroundColor() passed via uniform buffer
    outColor = ubo.backgroundColor;
}
