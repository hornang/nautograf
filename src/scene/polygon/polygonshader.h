#pragma once

#include <QSGMaterialShader>

class PolygonShader : public QSGMaterialShader
{
public:
    PolygonShader();
    bool updateUniformData(RenderState &state,
                           QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;
};
