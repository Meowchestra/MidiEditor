#version 460

// Per-vertex attributes (text quad vertices)
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

// Per-instance attributes (text data)
layout(location = 2) in vec4 instancePosSize; // x, y, width, height
layout(location = 3) in vec4 instanceColor;   // r, g, b, a
layout(location = 4) in vec4 instanceUV;      // u1, v1, u2, v2

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
    // Transform quad vertex to instance position and size
    vec2 scaledPos = position * instancePosSize.zw + instancePosSize.xy;
    
    // Convert from screen coordinates to normalized device coordinates
    vec2 ndc = (scaledPos / ubo.screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y; // Flip Y coordinate
    
    gl_Position = vec4(ndc, 0.0, 1.0);
    
    // Interpolate UV coordinates from font atlas
    vec2 uvRange = instanceUV.zw - instanceUV.xy;
    fragTexCoord = instanceUV.xy + texCoord * uvRange;
    
    fragColor = instanceColor;
}
