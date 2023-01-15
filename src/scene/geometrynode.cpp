#include "geometrynode.h"

GeometryNode::GeometryNode(const QString &id)
    : m_id(id)
{
}

const QString &GeometryNode::id() const
{
    return m_id;
}
