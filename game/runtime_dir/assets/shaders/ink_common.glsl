
#ifndef INK_COMMON_GLSL
#define INK_COMMON_GLSL

const uint TILE_SIZE    = 16u;
const uint MAX_PER_TILE = 64u;


struct InkParticle {
    vec4 posRadius; // world pos
    vec4 color;
};

#endif
