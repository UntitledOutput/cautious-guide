#version 450 core
// One-time bake per paintable mesh: rasterize in UV2 space so every texel knows its
// object-space position and normal. Not a compute shader; it needs the rasterizer.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec2 aUV2;

out vec3 vObjPos;
out vec3 vObjNormal;

void main() {
    vObjPos    = aPosition;
    vObjNormal = aNormal;
    gl_Position = vec4(aUV2 * 2.0 - 1.0, 0.0, 1.0);
}
