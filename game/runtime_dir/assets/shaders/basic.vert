#version 440 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

layout(std140) uniform Camera
{
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 camPos;
};

layout(std140) uniform Object
{
    mat4 model;
};

out vec3 vNormal;
out vec2 vUV;

void main()
{
    vNormal = mat3(model) * aNormal; // fine while scale is uniform
    vUV = aUV;
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}