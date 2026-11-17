#version 450 core

in vec3 vPosition;
in vec3 vNormal;

layout(location = 0) out vec4 oPosition;
layout(location = 1) out vec4 oNormal;

void main()
{
    oPosition = vec4(vPosition, 1.0);
    oNormal = vec4(normalize(vNormal), 1.0);
}