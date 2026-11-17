// paste into the surface shader that draws paintable meshes
// paintMap = PaintableObject::displayTexture, uv = uv1

const float INK_THRESHOLD = 0.5;

vec2 sample_paint(sampler2D paintMap, vec2 uv)
{
    ivec2 size = textureSize(paintMap, 0);
    vec2 p = uv * vec2(size) - 0.5;
    ivec2 i0 = ivec2(floor(p));
    vec2 f = fract(p);

    float weights[4] = float[4]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
    ivec2 offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));

    // team ids can't be filtered, so weight coverage per team and take the strongest
    float teamCoverage[4] = float[4](0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 4; ++i)
    {
        vec2 t = texelFetch(paintMap, clamp(i0 + offsets[i], ivec2(0), size - 1), 0).rg;
        teamCoverage[int(t.g * 3.0 + 0.5)] += weights[i] * t.r;
    }

    int team = 0;
    for (int i = 1; i < 4; ++i)
        if (teamCoverage[i] > teamCoverage[team])
            team = i;

    return vec2(teamCoverage[team], float(team));
}
