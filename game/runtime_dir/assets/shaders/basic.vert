#version 440 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in uvec4 aJoints;
layout (location = 4) in vec4 aWeights;

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
    uvec4 skin; // x = first joint, y = joint count
};

layout(std430, binding = 3) readonly buffer Joints
{
    mat4 joints[];
};

out vec3 vNormal;
out vec2 vUV;

void main()
{
    mat4 m = model;
    if (skin.y > 0u)
    {
        m *= aWeights.x * joints[skin.x + aJoints.x]
           + aWeights.y * joints[skin.x + aJoints.y]
           + aWeights.z * joints[skin.x + aJoints.z]
           + aWeights.w * joints[skin.x + aJoints.w];
    }

    vNormal = mat3(m) * aNormal;
    vUV = aUV;
    gl_Position = viewProj * m * vec4(aPos, 1.0);
}