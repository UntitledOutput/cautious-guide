#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

layout(std140) uniform Camera { mat4 view; mat4 proj; mat4 viewProj; vec4 camPos; };

struct Particle { vec4 positionLife; vec4 velocitySize; vec4 color; };
layout(std430) readonly buffer Instances { Particle particles[]; };

out vec2 vUV;
out vec4 vColor;

void main()
{
    Particle p = particles[gl_InstanceID];
    vUV = aUV;
    vColor = p.color;
    if (p.positionLife.w <= 0.0) { gl_Position = vec4(0, 0, 2, 1); return; } // clipped

    vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 up    = vec3(view[0][1], view[1][1], view[2][1]);
    vec3 world = p.positionLife.xyz + (right * aPos.x + up * aPos.y) * p.velocitySize.w;

    vColor.a *= clamp(p.positionLife.w * 4.0, 0.0, 1.0); // fade out over the last 0.25s
    gl_Position = viewProj * vec4(world, 1.0);
}
