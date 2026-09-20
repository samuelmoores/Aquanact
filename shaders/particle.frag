#version 330 core

in vec4 particleColor;
in float particleAge;
in float particleShapeVariation;
in vec2 rainScreenDirection;
in float rainProjectionScale;
in float particleLightBrightness;
out vec4 FragColor;

uniform int particleVisualShape;
uniform sampler2D splashTexture;
uniform float splashBrightness;
uniform float splashOpacity;

vec4 splashSpriteColor()
{
    const int frameCount = 5;
    float normalizedAge = clamp(particleAge, 0.0, 0.999999);
    int frame = int(floor(normalizedAge * float(frameCount)));

    // Preserve the placement authored inside each 16x16 cell. Later splash
    // frames intentionally occupy the top of the cell so they rise above the
    // impact point. StbImage stores the PNG's top row first and point-sprite
    // coordinates also begin at the top, so the Y coordinate is used directly.
    vec2 frameUv = vec2(
        (gl_PointCoord.x + float(frame)) / float(frameCount),
        gl_PointCoord.y);
    return texture(splashTexture, frameUv);
}

float softCircleAlpha(vec2 pointCoordinate)
{
    float distanceFromCenter = distance(pointCoordinate, vec2(0.5));
    return 1.0 - smoothstep(0.30, 0.50, distanceFromCenter);
}

float rainStreakAlpha(vec2 pointCoordinate)
{
    // Rotate the procedural capsule into the projected world-down direction.
    // Per-particle variation changes both its width and length without
    // requiring a texture or a second draw path.
    vec2 point = pointCoordinate * 2.0 - 1.0;
    vec2 direction = normalize(rainScreenDirection);
    point = vec2(dot(point, vec2(-direction.y, direction.x)),
                 dot(point, direction));
    float radius = mix(0.075, 0.14, particleShapeVariation);
    float fullHalfLength = mix(0.58, 0.88, particleShapeVariation);
    float halfLength = fullHalfLength * rainProjectionScale;
    vec2 start = vec2(0.0, -halfLength);
    vec2 segment = vec2(0.0, halfLength * 2.0);
    vec2 fromStart = point - start;
    float segmentLengthSquared = dot(segment, segment);
    float along = segmentLengthSquared > 0.000001
        ? clamp(dot(fromStart, segment) / segmentLengthSquared, 0.0, 1.0)
        : 0.0;
    float distanceFromStreak = length(fromStart - segment * along);
    return 1.0 - smoothstep(radius * 0.55, radius, distanceFromStreak);
}

void main()
{
    if (particleVisualShape == 2)
    {
        vec4 sprite = splashSpriteColor();
        // Splash opacity is independent of the weather color alpha. At 1.0,
        // the sprite uses its authored alpha unchanged; at 0.0 it is invisible.
        float alpha = sprite.a * splashOpacity;
        if (alpha <= 0.001) discard;
        FragColor = vec4(sprite.rgb * particleColor.rgb * particleLightBrightness * splashBrightness,
                         alpha);
        return;
    }

    float alpha = particleVisualShape == 1
        ? rainStreakAlpha(gl_PointCoord)
        : softCircleAlpha(gl_PointCoord);
    alpha *= 1.0 - particleAge;
    if (alpha <= 0.001) discard;
    FragColor = vec4(particleColor.rgb, particleColor.a * alpha);
}
