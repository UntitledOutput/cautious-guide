#version 440 core
out vec4 FragColor;

in vec2 vUV;

uniform sampler2D uTexture;

void main()
{
    FragColor = texture(uTexture, vUV);
}