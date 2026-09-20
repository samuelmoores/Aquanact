#version 330 core

// Original by localthunk (https://www.playbalatro.com)

/*
--- shader inputs --
uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform float     iTime;                 // shader playback time (in seconds)
uniform float     iTimeDelta;            // render time (in seconds)
uniform float     iFrameRate;            // shader frame rate
uniform int       iFrame;                // shader playback frame
uniform float     iChannelTime[4];       // channel playback time (in seconds)
uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube
uniform vec4      iDate;                 // (year, month, day, time in seconds)
*/

uniform vec3 iResolution;
uniform float iTime;

// Configuration (modify these values to change the effect)
uniform float SPIN_ROTATION;
uniform float SPIN_SPEED;
uniform vec2 OFFSET;
uniform vec4 COLOUR_1;
uniform vec4 COLOUR_2;
uniform vec4 COLOUR_3;
uniform float CONTRAST;
uniform float LIGTHING;
uniform float SPIN_AMOUNT;
uniform float PIXEL_FILTER;
uniform float SPIN_EASE;
uniform bool IS_ROTATE;
uniform float SHAPE_SCALE;
uniform float RADIAL_TWIST;
uniform float WARP_STRENGTH;
uniform float WARP_FREQUENCY;
uniform int WARP_ITERATIONS;
uniform float BAND_WIDTH;
uniform float LIGHTING_THRESHOLD;
uniform float BACKGROUND_BLEND;
uniform float LOOP_TIME;

vec4 effect(vec2 screenSize, vec2 screen_coords) {
    // A cosine phase returns to the exact same value with zero velocity at
    // both ends of the loop, avoiding a visible seam when LOOP_TIME wraps.
    float loop_phase = 6.28318530718 * fract(iTime / max(LOOP_TIME, 0.001));
    float animation_time = LOOP_TIME > 0.0
        ? LOOP_TIME * (0.5 - 0.5*cos(loop_phase))
        : iTime;
    float pixel_size = length(screenSize.xy) / max(abs(PIXEL_FILTER), 1.0);
    vec2 uv = (floor(screen_coords.xy*(1./pixel_size))*pixel_size - 0.5*screenSize.xy)/length(screenSize.xy) - OFFSET;
    float uv_len = length(uv);
    
    float speed = (SPIN_ROTATION*SPIN_EASE*0.2);
    if(IS_ROTATE){
       speed = animation_time * speed;
    }
    speed += 302.2;
    float new_pixel_angle = atan(uv.y, uv.x) + speed - SPIN_EASE*RADIAL_TWIST*(1.*SPIN_AMOUNT*uv_len + (1. - 1.*SPIN_AMOUNT));
    vec2 mid = (screenSize.xy/length(screenSize.xy))/2.;
    uv = (vec2((uv_len * cos(new_pixel_angle) + mid.x), (uv_len * sin(new_pixel_angle) + mid.y)) - mid);
    
    uv *= SHAPE_SCALE;
    speed = animation_time*(SPIN_SPEED);
    vec2 uv2 = vec2(uv.x+uv.y);
    
    for(int i=0; i < 8; i++) {
        if (i >= WARP_ITERATIONS) break;
        uv2 += sin(max(uv.x, uv.y) * WARP_FREQUENCY) + uv;
        uv  += WARP_STRENGTH*vec2(cos(5.1123314 + 0.353*uv2.y + speed*0.131121),sin(uv2.x - 0.113*speed));
        uv  -= WARP_STRENGTH*2.0*cos(uv.x + uv.y) - WARP_STRENGTH*2.0*sin(uv.x*0.711 - uv.y);
    }
    
    float safe_contrast = max(abs(CONTRAST), 0.001);
    float contrast_mod = (0.25*safe_contrast + 0.5*SPIN_AMOUNT + 1.2);
    float paint_res = min(2., max(0.,length(uv)*(0.035)*contrast_mod));
    float c1p = max(0.,1. - contrast_mod*abs(1.-paint_res));
    float c2p = max(0.,1. - contrast_mod*abs(paint_res));
    float c3p = 1. - min(1., c1p + c2p);
    float light = (LIGTHING - 0.2)*max(c1p*BAND_WIDTH - LIGHTING_THRESHOLD, 0.) + LIGTHING*max(c2p*BAND_WIDTH - LIGHTING_THRESHOLD, 0.);
    return (BACKGROUND_BLEND/safe_contrast)*COLOUR_1 + (1. - BACKGROUND_BLEND/safe_contrast)*(COLOUR_1*c1p + COLOUR_2*c2p + vec4(c3p*COLOUR_3.rgb, c3p*COLOUR_1.a)) + light;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord/iResolution.xy;
    
    fragColor = effect(iResolution.xy, uv * iResolution.xy);
}

out vec4 FragColor;

void main() {
    mainImage(FragColor, gl_FragCoord.xy);
}
