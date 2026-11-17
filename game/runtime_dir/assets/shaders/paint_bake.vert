#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 5) in vec2 aUV1;

layout(std140) uniform Object
{
    mat4 model;
    uvec4 skin;
};

out vec3 vPosition;
out vec3 vNormal;

void main()
{
    vPosition = (model * vec4(aPosition, 1.0)).xyz;
    vNormal = mat3(model) * aNormal;
    gl_Position = vec4(aUV1 * 2.0 - 1.0, 0.0, 1.0);
}