#pragma once

#include <QSGTransformNode>

class MaterialCreator;
class QSGTexture;
class QQuickWindow;

class RootNode : public QSGTransformNode
{
public:
    RootNode(const QImage &symbolImage,
             const QImage &fontImage,
             const QQuickWindow *window);
    ~RootNode();
    MaterialCreator *materialCreator() const;

private:
    MaterialCreator *m_materialCreator = nullptr;
};
