#version 440

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 colorIn;

layout(location = 0) out vec4 color;

layout(std140, binding = 0) uniform buf {
     mat4 qt_Matrix;
     float qt_Opacity;
 } ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    gl_Position = ubuf.qt_Matrix * vec4(aPos.xyz, 1.0);
    color = colorIn;
}
