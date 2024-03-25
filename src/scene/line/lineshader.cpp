#include "lineshader.h"
#include "linematerial.h"
#include "linenode.h"

bool LineShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    bool changed = false;
    QByteArray *uniformBuffer = state.uniformData();
    Q_ASSERT(uniformBuffer->size() == 136);

    if (state.isMatrixDirty()) {
        memcpy(uniformBuffer->data(), state.modelViewMatrix().constData(), 64);
        memcpy(uniformBuffer->data() + 64, state.projectionMatrix().constData(), 64);
        changed = true;
    }

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(uniformBuffer->data() + 128, &opacity, 4);
        changed = true;
    }

    LineMaterial *material = static_cast<LineMaterial *>(newMaterial);
    if (oldMaterial != newMaterial || material->uniforms.dirty) {
        memcpy(uniformBuffer->data() + 132, &material->uniforms.width, 4);
        material->uniforms.dirty = false;
        changed = true;
    }

    return changed;
}
