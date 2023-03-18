#version 440

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aNormal;
layout(location = 2) in vec3 colorIn;

layout(location = 1) out vec3 color;
layout(location = 2) out vec2 normal;

layout(std140, binding = 0) uniform buf {
    mat4 modelView;
    mat4 projection;
    float qt_Opacity;
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    float u_linewidth = 2.5;
    vec4 delta = vec4(0.5 * aNormal * u_linewidth, 0, 0);
    vec4 center = ubuf.modelView * vec4(aPos.x, aPos.y, 0.0, 1.0);

    gl_Position = ubuf.projection * (center + delta);
    color = colorIn;
    normal = aNormal;
}
