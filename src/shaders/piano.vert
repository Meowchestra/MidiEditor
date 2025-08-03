#version 460

// Per-vertex attributes (quad vertices)
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

// Per-instance attributes (piano key data) - optimized to 4 attributes max
layout(location = 2) in vec4 keyData;    // x, y, width, height
layout(location = 3) in vec4 keyColor;   // r, g, b, keyType (packed in alpha channel)

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
layout(location = 2) out float fragKeyType;

void main() {
    // Transform quad vertex to piano key position and size
    vec2 scaledPos = position * keyData.zw + keyData.xy;

    // Use MVP matrix for consistent coordinate transformation
    gl_Position = ubo.mvpMatrix * vec4(scaledPos, 0.0, 1.0);
    
    // Pass data to fragment shader (keyType packed in keyColor.a)
    fragColor = vec4(keyColor.rgb, 1.0);
    fragTexCoord = texCoord;
    fragKeyType = keyColor.a; // Extract keyType from alpha channel
}
