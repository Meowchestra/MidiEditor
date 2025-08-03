#version 460

// Vertex attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

// Instance attributes (per character) - MUST MATCH existing text shader
layout(location = 2) in vec4 instancePosSize; // x, y, width, height
layout(location = 3) in vec4 instanceColor;   // r, g, b, a
layout(location = 4) in vec4 instanceUV;      // u1, v1, u2, v2

// Uniform buffer (MUST MATCH other shaders exactly)
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

// Outputs to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragAtlasUV;

void main() {
    // Transform vertex position to character space
    vec3 charPos = position;
    charPos.xy *= instancePosSize.zw; // Scale by character width, height
    charPos.xy += instancePosSize.xy; // Translate to character position

    // Use MVP matrix for consistent coordinate transformation
    gl_Position = ubo.mvpMatrix * vec4(charPos, 1.0);
    
    // Calculate atlas UV coordinates
    vec2 atlasUV = mix(instanceUV.xy, instanceUV.zw, uv);
    
    // Pass data to fragment shader
    fragColor = instanceColor;
    fragAtlasUV = atlasUV;
}
