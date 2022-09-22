#pragma once

#include <QSGNode>

class SymbolImage;
class FontImage;
class QSGTexture;
class QQuickWindow;

class RootNode : public QSGNode
{
public:
    RootNode(const SymbolImage &symbolImage,
             const FontImage &fontImage,
             QQuickWindow *window);
    ~RootNode();
    QSGTexture *symbolTexture() const { return m_symbolTexture; }
    QSGTexture *fontTexture() const { return m_fontTexture; }

private:
    QSGTexture *m_symbolTexture = nullptr;
    QSGTexture *m_fontTexture = nullptr;
};
