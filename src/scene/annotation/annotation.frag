#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 color;
layout(location = 2) in float zoomLimit;

layout(std140, binding = 0) uniform buf {
    mat4 modelView;
    mat4 projection;
    float qt_Opacity;
    float scale;
} ubuf;

layout(binding = 1) uniform sampler2D ourTexture;
layout(location = 0) out vec4 fragColor;

const float smoothing = 6.0 / 16.0;
const float outlineWidth = 4.0 / 16.0;
const float edgeCenter = 0.5;
const float outerEdgeCenter = edgeCenter - outlineWidth;
const vec4 u_outlineColor = vec4(1, 1, 1, 1);

void main()
{
    // Replace conditional with smoothstep
    if (ubuf.scale < zoomLimit) {
        fragColor = vec4(0, 0, 0, 0);
    } else {
        float dist = texture(ourTexture, vTexCoord).r;
        float alpha = smoothstep(outerEdgeCenter - smoothing, outerEdgeCenter + smoothing, dist) * ubuf.qt_Opacity;
        float border = smoothstep(edgeCenter - smoothing, edgeCenter + smoothing, dist);
        fragColor = vec4(mix(u_outlineColor.rgb, color.rgb, border) * alpha, alpha);
    }
}
