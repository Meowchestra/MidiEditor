#version 460

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Font atlas texture
layout(binding = 1) uniform sampler2D fontAtlas;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Render text exactly like software QPainter::drawText()
    // Sample the alpha channel from the font atlas
    float alpha = texture(fontAtlas, fragTexCoord).r;
    
    // Apply the text color with the font atlas alpha
    // This matches how QPainter renders text with anti-aliasing
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
    
    // Discard fully transparent pixels to avoid artifacts
    if (outColor.a < 0.01) {
        discard;
    }
}
