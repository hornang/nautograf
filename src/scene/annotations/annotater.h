#pragma once

#include <memory>

#include <QLocale>
#include <QPointF>
#include <QRectF>

#include "scene/annotations/fontimage.h"
#include "scene/annotations/types.h"
#include "symbolimage.h"
#include <tilefactory/chart.h>

class Annotater
{
public:
    Annotater(std::shared_ptr<const FontImage> &fontImage,
              std::shared_ptr<const SymbolImage> &symbolImage,
              int pixelsPerLon);

    struct Annotations
    {
        std::vector<AnnotationSymbol> symbols;
        std::vector<AnnotationLabel> labels;

        Annotations &operator+=(const Annotations &other)
        {
            size_t initialSymbolsSize = symbols.size();

            symbols.insert(symbols.end(),
                           other.symbols.begin(),
                           other.symbols.end());

            size_t initialLabelsSize = labels.size();

            labels.insert(labels.end(),
                          other.labels.begin(),
                          other.labels.end());
            for (int i = initialLabelsSize; i < labels.size(); i++) {
                if (labels[i].parentSymbolIndex.has_value()) {
                    labels[i].parentSymbolIndex.value() += initialSymbolsSize;
                }
            }

            return *this;
        }
    };

    Annotations getAnnotations(const std::vector<std::shared_ptr<Chart>> &charts);

private:
    template <typename T>
    Annotations getAnnotations(
        const typename capnp::List<T>::Reader &elements,
        std::function<std::optional<TextureSymbol>(const typename T::Reader &)> getSymbol,
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
