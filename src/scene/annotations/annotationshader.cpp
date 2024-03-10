#include "annotationshader.h"
#include "annotationmaterial.h"
#include "annotationnode.h"

bool AnnotationShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
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

    auto *annotationMaterial = static_cast<AnnotationMaterial *>(newMaterial);
    if (oldMaterial != newMaterial || annotationMaterial->uniforms.dirty) {
        memcpy(uniformBuffer->data() + 132, &annotationMaterial->uniforms.zoom, 4);
        annotationMaterial->uniforms.dirty = false;
        changed = true;
    }

    return changed;
}

void AnnotationShader::updateSampledImage(QSGMaterialShader::RenderState &state,
                                          int binding,
                                          QSGTexture **texture,
                                          QSGMaterial *newMaterial,
                                          QSGMaterial *oldMaterial)
{

    auto *material = static_cast<AnnotationMaterial *>(newMaterial);

    if (*texture != material->texture()) {
        if (material->texture()) {
            *texture = material->texture();
            (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        }
    }
}
