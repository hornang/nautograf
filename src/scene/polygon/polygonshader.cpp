#include "polygonshader.h"
#include "polygonmaterial.h"

PolygonShader::PolygonShader()
{
    setShaderFileName(VertexStage, QLatin1String(":/scene/polygon/polygon.vert.qsb"));
    setShaderFileName(FragmentStage, QLatin1String(":/scene/polygon/polygon.frag.qsb"));
}

bool PolygonShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    bool changed = false;
    QByteArray *uniformBuffer = state.uniformData();
    Q_ASSERT(uniformBuffer->size() == 68);

    if (state.isMatrixDirty()) {
        memcpy(uniformBuffer->data(), state.combinedMatrix().constData(), 64);
        changed = true;
    }

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(uniformBuffer->data() + 64, &opacity, 4);
        changed = true;
    }

    return changed;
}
