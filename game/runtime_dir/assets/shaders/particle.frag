#version 450 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

void main()
{
    float a = vColor.a * smoothstep(1.0, 0.6, length(vUV * 2.0 - 1.0));
    if (a < 0.01) discard;
    FragColor = vec4(vColor.rgb, a);
}
