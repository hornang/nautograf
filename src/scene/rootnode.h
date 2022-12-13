#pragma once

#include <QSGTransformNode>

class QSGTexture;
class QQuickWindow;
class PolygonMaterial;
class AnnotationMaterial;

class RootNode : public QSGTransformNode
{
public:
    RootNode(const QImage &symbolImage,
             const QImage &fontImage,
             const QQuickWindow *window);
    ~RootNode();
    QSGTexture *symbolTexture() const { return m_symbolTexture; }
    QSGTexture *fontTexture() const { return m_fontTexture; }
    PolygonMaterial *polygonMaterial() const { return m_polygonMaterial; }
    PolygonMaterial *blendColorMaterial() const { return m_blendColorMaterial; }
    AnnotationMaterial *symbolMaterial() const { return m_symbolMaterial; }
    AnnotationMaterial *textMaterial() const { return m_textMaterial; }

private:
    QSGTexture *m_symbolTexture = nullptr;
    QSGTexture *m_fontTexture = nullptr;
    PolygonMaterial *m_polygonMaterial = nullptr;
    PolygonMaterial *m_blendColorMaterial = nullptr;
    AnnotationMaterial *m_symbolMaterial = nullptr;
    AnnotationMaterial *m_textMaterial = nullptr;
};
