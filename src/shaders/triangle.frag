#version 460

// Inputs from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Use solid colors exactly like software drawPolygon (no gradients or 3D effects)
    outColor = fragColor;
}
