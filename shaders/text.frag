#version 450
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D fontTexture;
void main() {
    float alpha = texture(fontTexture, inTexCoord).r;
    outColor = vec4(1.0, 1.0, 1.0, alpha); // White text
    //outColor = vec4(alpha, alpha, alpha, 0.5); // White text
}