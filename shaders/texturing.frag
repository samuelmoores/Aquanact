#version 330
// A fragment shader for rendering a mesh that has a texture, but no lighting.
layout (location=0) out vec4 FragColor;

// Input from vertices: interpolated texture coordinate.
in vec2 TexCoord;

// Uniform from application: the texture sampler.
uniform sampler2D baseTexture;
uniform bool hasBaseTexture;

void main() {
    // Sample the texture image at the fragment's texture coordinate.
    FragColor = hasBaseTexture
        ? texture(baseTexture, TexCoord)
        : vec4(0.20, 0.20, 0.20, 1.0);
}
