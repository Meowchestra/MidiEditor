#version 460

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float fragKeyType;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Render piano keys exactly like software QPainter::fillRect()
    // The software renderer uses simple solid colors from Appearance class
    // No gradients, shadows, or special effects - just solid color rectangles
    
    outColor = fragColor;
}
