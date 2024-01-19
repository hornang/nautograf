#pragma once

#include <QHash>
#include <QImage>

class QSGTexture;

namespace ftgl {
struct texture_atlas_t;
struct texture_font_t;
}

class FontImage
{

public:
    FontImage();
    ~FontImage();
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
    struct FontGlyphs
    {
        FontType type;
        struct texture_font_t *textureFont;
        QHash<QChar, struct texture_glyph_t *> glyphs;
    };

    static QString locateFontFile(FontType type);
    QImage m_image;
    QHash<FontType, FontGlyphs> m_glyphs;
    struct texture_atlas_t *m_textureAtlas = nullptr;
};
