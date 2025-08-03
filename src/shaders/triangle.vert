#version 460

// Vertex attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

// Instance attributes (per triangle)
layout(location = 2) in vec4 triangleTransform; // x, y, size, rotation
layout(location = 3) in vec4 triangleColor;     // r, g, b, a

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
layout(location = 1) out vec2 fragUV;

void main() {
    // Transform vertex position to triangle space
    vec3 trianglePos = position;
    
    // Apply rotation if needed
    float rotation = triangleTransform.w;
    if (rotation != 0.0) {
        float cosR = cos(rotation);
        float sinR = sin(rotation);
        vec2 rotated = vec2(
            trianglePos.x * cosR - trianglePos.y * sinR,
            trianglePos.x * sinR + trianglePos.y * cosR
        );
        trianglePos.xy = rotated;
    }
    
    trianglePos.xy *= triangleTransform.z; // Scale by size
    trianglePos.xy += triangleTransform.xy; // Translate to position

    // Use MVP matrix for consistent coordinate transformation
    gl_Position = ubo.mvpMatrix * vec4(trianglePos, 1.0);
    
    // Pass data to fragment shader
    fragColor = triangleColor;
    fragUV = uv;
}
