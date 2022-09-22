#pragma once

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>

class AnnotationShader : public QSGMaterialShader
{
public:
    AnnotationShader()
    {
        setShaderFileName(VertexStage, QLatin1String(":/src/scene/annotation.vert.qsb"));
        setShaderFileName(FragmentStage, QLatin1String(":/src/scene/annotation.frag.qsb"));
    }
    bool updateUniformData(RenderState &state,
                           QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;

    void updateSampledImage(QSGMaterialShader::RenderState &state,
                            int binding,
                            QSGTexture **texture,
                            QSGMaterial *newMaterial,
                            QSGMaterial *oldMaterial) override;
};
