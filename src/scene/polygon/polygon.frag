#version 440

layout(location = 0) in vec4 color;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
} ubuf;

void main()
{
    float opacity = ubuf.qt_Opacity * color.a;
    fragColor = vec4(color.rgb * opacity, opacity);
}
