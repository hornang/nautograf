#pragma once

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>

class LineShader : public QSGMaterialShader
{
public:
    LineShader()
    {
        setShaderFileName(VertexStage, QLatin1String(":/scene/line/line.vert.qsb"));
        setShaderFileName(FragmentStage, QLatin1String(":/scene/line/line.frag.qsb"));
    }
    bool updateUniformData(RenderState &state,
                           QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;
};
