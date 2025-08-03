#version 460

// Inputs from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Simple triangle rendering with antialiasing
    vec4 finalColor = fragColor;
    
    // Apply subtle gradient for 3D effect
    float gradient = 1.0 - (fragUV.y * 0.2);
    finalColor.rgb *= gradient;
    
    outColor = finalColor;
}
