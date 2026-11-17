#version 440 core
in vec3 vNormal;
in vec2 vUV;
in vec2 vUV1;
out vec4 FragColor;

layout(std140) uniform Material
{
    vec4 baseColor;
    vec4 paintColor0;
    vec4 paintColor1;
    vec4 paintColor2;
    vec4 paintColor3;
    int usePaintMap;
};
uniform sampler2D _a0;
uniform sampler2D paintMap;

const float INK_THRESHOLD = 0.5;

vec2 sample_paint(sampler2D tex, vec2 uv)
{
    ivec2 size = textureSize(tex, 0);
    vec2 p = uv * vec2(size) - 0.5;
    ivec2 i0 = ivec2(floor(p));
    vec2 f = fract(p);

    float weights[4] = float[4]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
    ivec2 offsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));

    float teamCoverage[4] = float[4](0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 4; ++i)
    {
        vec2 t = texelFetch(tex, clamp(i0 + offsets[i], ivec2(0), size - 1), 0).rg;
        teamCoverage[int(t.g * 3.0 + 0.5)] += weights[i] * t.r;
    }

    int team = 0;
    for (int i = 1; i < 4; ++i)
        if (teamCoverage[i] > teamCoverage[team])
            team = i;

    return vec2(teamCoverage[team], float(team));
}

void main()
{
    float light = max(dot(normalize(vNormal), normalize(vec3(0.4, 1.0, 0.3))), 0.0) * 0.8 + 0.2;
    vec4 albedo = texture(_a0, vUV);

    if (usePaintMap != 0)
    {
        vec2 ink = sample_paint(paintMap, vUV1);
        vec4 colors[4] = vec4[4](paintColor0, paintColor1, paintColor2, paintColor3);
        float w = fwidth(ink.x);
        albedo.rgb = mix(albedo.rgb, colors[int(ink.y)].rgb, smoothstep(INK_THRESHOLD - w, INK_THRESHOLD + w, ink.x));
    }

    FragColor = vec4(albedo.rgb * light, albedo.a);
}