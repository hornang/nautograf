#pragma once

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <optional>

struct TextureSymbol
{
    QRectF coords;
    QSize size;

    // This is the pixel position within the symbol that should be placed on
    // geographical position in the map. E.g for a stake the bottom of the
    // symbol is to be placed, not the center.
    QPointF center;

    // Rectangle within the symbol which should not be overlapped
    // For many symbols it is simply QRectF(0, 0, size.width(), size.height())
    QRectF roi;
    QColor color;

    bool operator!=(const TextureSymbol &other) const
    {
        return (this->coords != other.coords || this->size != other.size);
    }

    bool operator==(const TextureSymbol &other) const
    {
        return (this->coords == other.coords && this->size == other.size);
    }
};

enum class FontType {
    Unknown,
    Soundings,
    Normal,
};

// This only applies to symbol collisions. Labels are always checked.
enum class CollisionRule {
    NoCheck,
    Always,
    OnlyWithSameType
};

struct Label
{
    QString text;
    FontType font;
    QColor color;
    float pointSize;
};

struct AnnotationLabel
{
    Label label;
    QPointF pos;
    std::optional<size_t> parentSymbolIndex;
    QPointF offset;
    QRectF boundingBox;
    std::optional<float> minZoom;
};

struct AnnotationSymbol
{
    QPointF pos;
    std::optional<TextureSymbol> symbol;
    std::optional<float> minZoom;
    int priority = 0;
    CollisionRule collisionRule;
};
