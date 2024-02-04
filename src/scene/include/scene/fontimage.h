#pragma once

#include <QDir>
#include <QImage>
#include <QList>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QString>

#include <optional>

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdf-atlas-read.h>

class FontWorker;

class FontImage : public QObject
{
    Q_OBJECT

public:
    FontImage();
    ~FontImage();
    FontImage(const FontImage &other) = delete;

    enum class FontType {
        Unknown,
        Soundings,
        Normal,
    };

    struct Atlas
    {
        QImage image;
        std::unordered_map<FontType, msdf_atlas_read::FontGeometry> geometries;
    };

    QImage image() const
    {
        if (!m_atlas.has_value()) {
            return {};
        }
        return m_atlas->image;
    }
    QList<msdf_atlas_read::GlyphMapping> glyphs(const QString &text, float pixelSize, FontType type = FontType::Soundings) const;
    QSize atlasSize() const
    {
        if (!m_atlas.has_value()) {
            return {};
        }
        return m_atlas->image.size();
    }
    QRectF boundingBox(const QString &text, float pixelSize, FontType type = FontType::Soundings) const;
    static QString locateFontFile(FontType type);

signals:
    void atlasChanged();

public slots:
    void setAtlas(const FontImage::Atlas &cache);

private:
    FontWorker *m_worker = nullptr;
    std::optional<FontImage::Atlas> m_atlas;
};
