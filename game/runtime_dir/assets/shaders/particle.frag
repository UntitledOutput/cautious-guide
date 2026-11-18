#version 450 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

void main()
{
    float a = vColor.a * smoothstep(1.0, 0.6, length(vUV * 2.0 - 1.0));
    if (a < 0.01) discard;

    float l = dot(vec3(vUV,0), vec3(1,1,1));



    FragColor = vec4(l*vColor.rgb, 1);
}
