#version 460

// MINIMAL TEST: Only per-vertex attributes (no instance data)
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

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
layout(location = 1) out vec2 fragTexCoord;

void main() {
    // Transform quad vertex to instance position and size (exactly like software)
    vec2 scaledPos = position * instancePosSize.zw + instancePosSize.xy;

    // Use MVP matrix for consistent coordinate transformation (like software MatrixWidget)
    gl_Position = ubo.mvpMatrix * vec4(scaledPos, 0.0, 1.0);

    // Pass color and texture coordinates to fragment shader
    fragColor = instanceColor;
    fragTexCoord = texCoord;
}
