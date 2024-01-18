#pragma once

#include <QHash>
#include <QImage>

#include <msdf-atlas-gen/msdf-atlas-gen.h>

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
        Soundings,
        Normal,
    };

    struct Glyph
    {
        QRectF texture;
        QRectF target;
    };

    const QImage &image() const { return m_image; }
    QList<Glyph> glyphs(const QString &text, float pixelSize, FontType type = FontType::Soundings) const;
    QRectF boundingBox(const QString &text, float pixelSize, FontType type = FontType::Soundings) const;

private:
    static QString locateFontFile(FontType type);
    QImage m_image;
    std::vector<msdf_atlas::GlyphGeometry> m_glyphStorage;
    QHash<FontType, msdf_atlas::FontGeometry> m_glyphs;
};
