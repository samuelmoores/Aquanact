#version 330 core

layout(location = 0) in vec3 particlePosition;
layout(location = 1) in vec4 particleInputColor;
layout(location = 2) in float particleSize;
layout(location = 3) in float particleInputAge;
layout(location = 4) in float particleLifetime;
layout(location = 5) in float particleInputShapeVariation;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec2 viewportSize;
uniform int particleVisualShape;

struct ParticleSunLight {
    vec3 direction;
    vec3 color;
    float intensity;
    float ambient;
};
struct ParticlePointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
    float radiusFade;
    float constant;
    float linear;
    float quadratic;
};
uniform ParticleSunLight sunLight;
uniform int pointLightCount;
uniform ParticlePointLight pointLights[8];

out vec4 particleColor;
out float size;
out float particleAge;
out float particleShapeVariation;
out vec2 rainScreenDirection;
out float rainProjectionScale;
out float particleLightBrightness;

void main()
{
    particleColor = particleInputColor;
    size = particleSize;
    particleAge = clamp(particleInputAge / max(particleLifetime, 0.001), 0.0, 1.0);
    particleShapeVariation = particleInputShapeVariation;
    vec4 viewPosition = view * model * vec4(particlePosition, 1.0);
    vec3 worldPosition = (model * vec4(particlePosition, 1.0)).xyz;
    float sunLuminance = dot(sunLight.color, vec3(0.2126, 0.7152, 0.0722));
    float brightness = sunLight.ambient + sunLight.intensity * sunLuminance * 0.5;
    for (int lightIndex = 0; lightIndex < 8; ++lightIndex)
    {
        if (lightIndex >= pointLightCount) break;
        ParticlePointLight light = pointLights[lightIndex];
        vec3 toLight = light.position - worldPosition;
        float distanceToLight = length(toLight);
        float attenuation = 1.0 / max(light.constant + light.linear * distanceToLight +
            light.quadratic * distanceToLight * distanceToLight, 0.001);
        float radiusFactor = 1.0 - smoothstep(light.radius * light.radiusFade, light.radius,
            distanceToLight);
        float lightLuminance = dot(light.color, vec3(0.2126, 0.7152, 0.0722));
        brightness += lightLuminance * light.intensity * attenuation * radiusFactor;
    }
    // Lighting can only darken a splash. Bright areas preserve the authored
    // sprite color; shadow-like low-light areas reduce its brightness.
    particleLightBrightness = clamp(brightness, 0.15, 1.0);
    gl_Position = projection * viewPosition;

    // Rain is simulated along the world's vertical axis. Project that fixed
    // world-space direction at each particle so its streak does not remain
    // locked to screen-up when the camera pitches.
    vec3 viewDown = normalize(mat3(view) * vec3(0.0, -1.0, 0.0));
    vec4 clipDown = projection * vec4(viewDown, 0.0);
    vec2 screenDown = (clipDown.xy * gl_Position.w -
        gl_Position.xy * clipDown.w) * viewportSize;
    float screenDownLength = length(screenDown);
    // gl_PointCoord uses an upper-left origin by default, opposite clip
    // space's Y direction.
    rainScreenDirection = screenDownLength > 0.0001
        ? vec2(screenDown.x, -screenDown.y) / screenDownLength
        : vec2(0.0, 1.0);
    // A vertical streak becomes a compact drop when viewed along its axis.
    rainProjectionScale = clamp(length(viewDown.xy), 0.0, 1.0);
    // Keep the procedural point sprite visible without allowing close-up
    // particles to become expensive full-screen overdraw.
    float projectedSize = particleSize * viewportSize.y * projection[1][1] /
        max(-viewPosition.z, 0.01);
    float minimumPointSize = particleVisualShape == 1 ? 6.0 : 2.0;
    gl_PointSize = clamp(projectedSize, minimumPointSize, 256.0);
}
