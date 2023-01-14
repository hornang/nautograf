#include <freetype-gl/freetype-gl.h>
#include <freetype-gl/texture-font.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRawFont>
#include <QRectF>
#include <QSGTexture>
#include <QStandardPaths>

#include "fontimage.h"

constexpr float basePointSize = 20;

FontImage::FontImage()
{
    const QList<FontType> fontsToLoad = { FontType::Normal,
                                          FontType::Soundings };

    constexpr int atlasWidth = 512;
    constexpr int atlasHeight = 512;
    m_textureAtlas = texture_atlas_new(atlasWidth, atlasHeight, 1);

    const QString glyphsToRender(R"(ABCDEFGHIJKLMNOPQRSTUVWXYZÆØÅabcdefghijklmnopqrstuvwxyz1234567890æøå.,-? )");

    for (const auto &fontType : fontsToLoad) {
        const auto fontFile = locateFontFile(fontType);

        if (fontFile.isEmpty()) {
            qWarning() << "Unable to find a system font";
            continue;
        }

        qInfo() << "Found font file: " << fontFile;

        auto textureFont = texture_font_new_from_file(m_textureAtlas,
                                                      basePointSize,
                                                      fontFile.toUtf8().data());

        if (!textureFont) {
            qWarning() << "Failed to create texture font";
            continue;
        }

        textureFont->rendermode = RENDER_SIGNED_DISTANCE_FIELD;
        textureFont->padding = 2;
        QHash<QChar, struct texture_glyph_t *> glyphs;

        for (const QChar &c : glyphsToRender) {
            QString s = c;
            // I guess this functions reads as many bytes it needs to get a valid UTF-8 char
            glyphs[c] = texture_font_get_glyph(textureFont, s.toUtf8().data());
        }

        m_glyphs[fontType] = { fontType, textureFont, glyphs };
    }

    QImage image(m_textureAtlas->data, atlasWidth, atlasHeight, QImage::Format_Grayscale8);
    m_image = image;
}

QRectF FontImage::boundingBox(const QString &text, float pointSize, FontType type) const
{
    int yMin = std::numeric_limits<int>::max();
    int yMax = std::numeric_limits<int>::min();
    int xMax = std::numeric_limits<int>::min();

    auto glyphs = FontImage::glyphs(text, pointSize, type);

    for (const auto & glyph: glyphs) {
        yMin = std::min<float>(glyph.target.top(), yMin);
        yMax = std::max<float>(glyph.target.bottom(), yMax);
        xMax = std::max<float>(glyph.target.right(), xMax);
    }

    return QRectF(0, yMin, xMax, yMax - yMin);
}

QList<FontImage::Glyph> FontImage::glyphs(const QString &text,
                                          float pointSize,
                                          FontType type) const
{
    if (!m_glyphs.contains(type)) {
        return {};
    }

    float advance = 0;
    QString prevChar = 0;
    const float ratio = pointSize / basePointSize;
    QList<FontImage::Glyph> glyphs;

    for (int i = 0; i < text.length(); i++) {
        texture_glyph_t *glyph = m_glyphs[type].glyphs[text.at(i)];

        if (!glyph) {
            continue;
        }

        QPointF topLeft(advance + glyph->offset_x * ratio, -glyph->offset_y * ratio);
        QPointF bottomRight = topLeft + QPointF(glyph->width * ratio, glyph->height * ratio);
        QRectF texture(QPointF(glyph->s0, glyph->t0), QPointF(glyph->s1, glyph->t1));

        Glyph outputGlyph {
            texture,
            QRectF(topLeft, bottomRight)
        };

        glyphs.append(outputGlyph);
        advance += (glyph->advance_x + texture_glyph_get_kerning(glyph, prevChar.toUtf8().data())) * ratio;
        prevChar = text.at(i);
    }

    return glyphs;
}

QString FontImage::locateFontFile(FontType type)
{
#ifdef Q_OS_LINUX
    QDir dir("/usr/share/fonts/truetype/ubuntu");

    QStringList fonts;
    switch (type) {
    case FontType::Normal:
        fonts = dir.entryList({ "ubuntu-r*" });
        break;
    case FontType::Soundings:
        fonts = dir.entryList({ "ubuntu-ri*" });
        break;
    }
#else
    QDir dir(R"(C:\Windows\Fonts)");

    QStringList fonts;
    switch (type) {
    case FontType::Normal:
        fonts = dir.entryList({ "arial*" });
        break;
    case FontType::Soundings:
        fonts = dir.entryList({ "ariali*" });
        break;
    }
#endif
    if (fonts.isEmpty()) {
        return {};
    }

    return dir.filePath(fonts.first());
}

FontImage::~FontImage()
{
    for (const FontGlyphs &fontGlyphs : qAsConst(m_glyphs)) {
        texture_font_delete(fontGlyphs.textureFont);
    }

    if (m_textureAtlas) {
        texture_atlas_delete(m_textureAtlas);
    }
}
