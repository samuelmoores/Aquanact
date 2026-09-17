#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 view;
uniform mat4 projection;
uniform vec2 viewportSize;

in vec4 color[];
in float size[];
in float age[];
out vec2 uv;
out vec4 particleColor;
out float particleAge;

void main()
{
    vec3 center = gl_in[0].gl_Position.xyz;
    vec4 viewCenter = view * vec4(center, 1.0);
    // Limit the billboard to 256 pixels across. Large world-space particles
    // otherwise cause severe transparent overdraw when the camera is close.
    float maxViewHalfHeight = 128.0 * max(-viewCenter.z, 0.01) * 2.0 /
        (max(viewportSize.y, 1.0) * projection[1][1]);
    float billboardSize = min(size[0], maxViewHalfHeight);
    vec3 right = vec3(view[0]) * billboardSize;
    vec3 up = vec3(view[1]) * billboardSize;
    vec3 corners[4] = vec3[4](center - right - up, center + right - up,
                              center - right + up, center + right + up);
    vec2 coordinates[4] = vec2[4](vec2(0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0));
    for (int i = 0; i < 4; ++i)
    {
        uv = coordinates[i];
        particleColor = color[0];
        particleAge = age[0];
        gl_Position = projection * view * vec4(corners[i], 1.0);
        EmitVertex();
    }
    EndPrimitive();
}
