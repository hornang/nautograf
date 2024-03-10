#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 color;
layout(location = 2) in float minZoom;

layout(std140, binding = 0) uniform buf {
    mat4 modelView;
    mat4 projection;
    float qt_Opacity;
    float zoom;
} ubuf;

layout(binding = 1) uniform sampler2D ourTexture;
layout(location = 0) out vec4 fragColor;

const float smoothing = 2.0 / 16.0;
const float borderSmoothing = 3.0 / 16.0;
const float outlineWidth = 4.0 / 16.0;
const float edgeCenter = 0.5;
const float outerEdgeCenter = edgeCenter - outlineWidth;
const vec4 u_outlineColor = vec4(1, 1, 1, 1);

void main()
{
    float dist = texture(ourTexture, vTexCoord).r;
    float alpha = smoothstep(outerEdgeCenter - smoothing, outerEdgeCenter + smoothing, dist) * ubuf.qt_Opacity;
    float zoomRevealAlpha = smoothstep(0, 1, ((ubuf.zoom - minZoom) + 1));
    alpha *= zoomRevealAlpha;
    float border = smoothstep(edgeCenter - borderSmoothing, edgeCenter + borderSmoothing, dist);
    fragColor = vec4(mix(u_outlineColor.rgb, color.rgb, border) * alpha, alpha);
}
