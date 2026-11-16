// Shared paint helpers. Prepend after #version in every shader that touches the paint map
// (splat, dilate, query, and the surface fragment shader), so gameplay and visuals agree.
//
// Paint texel layout (GL_RG8):
//   r = coverage 0..1
//   g = team / 3   (team 0..3)
// Coverage 0 means unpainted regardless of team.

#ifndef PAINT_COMMON_GLSL
#define PAINT_COMMON_GLSL

const float INK_THRESHOLD = 0.5;
const int   MAX_TEAMS     = 4;

struct PaintSample {
    int   team;
    float coverage;
};

vec4 encodePaint(PaintSample s) {
    return vec4(s.coverage, float(s.team) / 3.0, 0.0, 0.0);
}

PaintSample decodePaint(vec4 texel) {
    PaintSample s;
    s.team     = clamp(int(texel.g * 3.0 + 0.5), 0, MAX_TEAMS - 1);
    s.coverage = texel.r;
    return s;
}

bool isInked(PaintSample s) {
    return s.coverage >= INK_THRESHOLD;
}

// Same team adds coverage, another team eats it first and takes over once it goes negative.
PaintSample applySplat(PaintSample s, int team, float amount) {
    if (s.coverage <= 0.0 || s.team == team) {
        s.team     = team;
        s.coverage = min(1.0, s.coverage + amount);
    } else {
        s.coverage -= amount;
        if (s.coverage < 0.0) {
            s.team     = team;
            s.coverage = min(1.0, -s.coverage);
        }
    }
    return s;
}

// Team ids can't be filtered, so bilinear is done by hand: each team gets the weighted
// coverage of the 4 neighbours and the strongest team wins.
PaintSample samplePaint(sampler2D paintMap, vec2 uv) {
    ivec2 size = textureSize(paintMap, 0);
    vec2  p    = uv * vec2(size) - 0.5;
    ivec2 i0   = ivec2(floor(p));
    vec2  f    = fract(p);

    ivec2 lo = ivec2(0);
    ivec2 hi = size - 1;
    vec4 t00 = texelFetch(paintMap, clamp(i0,               lo, hi), 0);
    vec4 t10 = texelFetch(paintMap, clamp(i0 + ivec2(1, 0), lo, hi), 0);
    vec4 t01 = texelFetch(paintMap, clamp(i0 + ivec2(0, 1), lo, hi), 0);
    vec4 t11 = texelFetch(paintMap, clamp(i0 + ivec2(1, 1), lo, hi), 0);

    float w[4]  = float[4]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
    vec4  tx[4] = vec4[4](t00, t10, t01, t11);

    float teamCov[MAX_TEAMS] = float[MAX_TEAMS](0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 4; ++i) {
        PaintSample s = decodePaint(tx[i]);
        teamCov[s.team] += w[i] * s.coverage;
    }

    PaintSample best;
    best.team     = 0;
    best.coverage = teamCov[0];
    for (int t = 1; t < MAX_TEAMS; ++t) {
        if (teamCov[t] > best.coverage) {
            best.team     = t;
            best.coverage = teamCov[t];
        }
    }
    return best;
}

#endif
