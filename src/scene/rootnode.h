#pragma once

#include <QSGTransformNode>

class QSGTexture;
class QQuickWindow;
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
    AnnotationMaterial *symbolMaterial() const { return m_symbolMaterial; }
    AnnotationMaterial *textMaterial() const { return m_textMaterial; }

private:
    QSGTexture *m_symbolTexture = nullptr;
    QSGTexture *m_fontTexture = nullptr;
    AnnotationMaterial *m_symbolMaterial = nullptr;
    AnnotationMaterial *m_textMaterial = nullptr;
};
