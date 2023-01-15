#pragma once

#include <QSGNode>
#include <QString>

class GeometryNode : public QSGNode
{
public:
    GeometryNode() = delete;
    GeometryNode(const QString &id);
    const QString &id() const;

private:
    QString m_id;
};
