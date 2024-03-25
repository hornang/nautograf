#version 440

layout(location = 1) in vec3 color;
layout(location = 2) in vec2 normal;

layout(std140, binding = 0) uniform buf {
    mat4 modelView;
    mat4 projection;
    float qt_Opacity;
    float width;
} ubuf;

layout(location = 0) out vec4 fragColor;

void main()
{
    float alpha = smoothstep(1.0, 0.0, length(normal)) * ubuf.qt_Opacity;
    fragColor = vec4(color.rgb * alpha, alpha) ;
}
