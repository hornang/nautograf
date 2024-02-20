#pragma once

#include <memory>

#include <QLocale>
#include <QPointF>
#include <QRectF>

#include "symbolimage.h"
#include <scene/fontimage.h>
#include <tilefactory/chart.h>

// This only applies to symbol collisions. Labels are always checked.
enum class CollisionRule {
    NoCheck,
    Always,
    OnlyWithSameType
};

struct Label
{
    QString text;
    FontImage::FontType font;
    QColor color;
    float pointSize;
};

struct AnnotationLabel
{
    Label label;
    QPointF pos;
    QPointF offset;
    QRectF boundingBox;
    std::optional<float> scaleLimit;
    std::optional<float> parentScaleLimit;
};

struct Annotation
{
    QPointF pos;
    std::optional<SymbolImage::TextureSymbol> symbol;
    std::optional<float> scaleLimit;
    QList<AnnotationLabel> labels;
    int priority = 0;
    CollisionRule collisionRule;
};

class Annotater
{
public:
    Annotater(std::shared_ptr<const FontImage> &fontImage,
              std::shared_ptr<const SymbolImage> &symbolImage,
              int pixelsPerLon);

    QList<Annotation> getAnnotations(const std::vector<std::shared_ptr<Chart>> &charts);

private:
    template <typename T>
    QList<Annotation> getAnnotations(
        const typename capnp::List<T>::Reader &elements,
        std::function<std::optional<SymbolImage::TextureSymbol>(const typename T::Reader &)> getSymbol,
        std::function<std::optional<ChartData::Position::Reader>(const typename T::Reader &)> getPosition,
        std::function<Label(const typename T::Reader &)> getLabel,
        int priority = 0,
        CollisionRule collissionRule = CollisionRule::NoCheck) const;
    QPointF posToMercator(const ChartData::Position::Reader &pos) const;
    QString getDepthString(float depth) const;

private:
    QLocale m_locale = QLocale::system();
    std::shared_ptr<const FontImage> m_fontImage;
    std::shared_ptr<const SymbolImage> m_symbolImage;
    int m_pixelsPerLon = 0;
};
