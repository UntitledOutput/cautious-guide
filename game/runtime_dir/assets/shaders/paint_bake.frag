#version 450 core

in vec3 vObjPos;
in vec3 vObjNormal;

layout(location = 0) out vec4 oPosition; // xyz = object pos, w = 1 inside a UV island
layout(location = 1) out vec4 oNormal;

void main() {
    oPosition = vec4(vObjPos, 1.0);
    oNormal   = vec4(normalize(vObjNormal), 1.0);
}
