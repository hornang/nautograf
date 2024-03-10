#version 440

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 offset;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 colorIn;
layout(location = 4) in float minZoomIn;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 color;
layout(location = 2) out float minZoom;

layout(std140, binding = 0) uniform buf {
    mat4 modelView;
    mat4 projection;
    float qt_Opacity;
    float zoom;
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    vec4 center = ubuf.modelView * vec4(aPos.x, aPos.y, 0.0, 1.0);
    center.x += offset.x;
    center.y += offset.y;

    gl_Position = ubuf.projection * center;

    vTexCoord = aTexCoord;
    minZoom = minZoomIn;
    color = colorIn;
}
