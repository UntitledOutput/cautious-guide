#version 440 core
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;

layout(std140) uniform Material
{
    vec4 baseColor;
};
uniform sampler2D _a0;

void main()
{
    float light = max(dot(normalize(vNormal), normalize(vec3(0.4, 1.0, 0.3))), 0.0) * 0.8 + 0.2;
    vec4 albedo = texture(_a0, vUV) * baseColor;
    FragColor = vec4(albedo.rgb * light, albedo.a);
}