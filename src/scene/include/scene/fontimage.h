#pragma once

#include <QDir>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QSize>
#include <QString>

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdf-atlas-read.h>

class FontImage
{

public:
    FontImage();
    FontImage(const FontImage &other) = delete;

    enum class FontType {
        Unknown,
        Soundings,
        Normal,
    };

    const QImage &image() const { return m_image; }
    QList<msdf_atlas_read::GlyphMapping> glyphs(const QString &text, float pixelSize, FontType type = FontType::Soundings) const;
    QSize atlasSize() const { return m_image.size(); }
    QRectF boundingBox(const QString &text, float pixelSize, FontType type = FontType::Soundings) const;
    static QString locateFontFile(FontType type);

private:
    bool createCache(const QDir &dir);
    bool loadCache(const QDir &dir);
    QImage m_image;
    msdf_atlas_read::AtlasProperties m_atlasProperties;
    std::unordered_map<FontType, msdf_atlas_read::FontGeometry> m_fontGeometries;
};
