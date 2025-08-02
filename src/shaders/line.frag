#version 460

// Input from vertex shader
layout(location = 0) in vec4 fragColor;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Render lines exactly like software QPainter::drawLine()
    // Simple solid color line with no effects
    outColor = fragColor;
}
