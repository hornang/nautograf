#pragma once

#include <QDir>
#include <QHash>
#include <QImage>

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdf-atlas-read.h>

class QSGTexture;

namespace ftgl {
struct texture_atlas_t;
struct texture_font_t;
}

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
