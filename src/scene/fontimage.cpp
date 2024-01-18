#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRawFont>
#include <QRectF>
#include <QSGTexture>
#include <QStandardPaths>
#include <QString>

#include "scene/fontimage.h"

using namespace msdf_atlas;

FontImage::FontImage()
{
    msdfgen::FreetypeHandle *freetype = msdfgen::initializeFreetype();

    if (freetype == nullptr) {
        qWarning() << "Unable to initialize freetype";
        return;
    }

    const QList<FontType> fontsToLoad = { FontType::Normal, FontType::Soundings };

    const QString glyphsToRender(R"(ABCDEFGHIJKLMNOPQRSTUVWXYZÆØÅabcdefghijklmnopqrstuvwxyz1234567890æøå.,-? )");

    Charset charset;

    for (const QChar &c : glyphsToRender) {
        charset.add(c.unicode());
    }

    for (const auto &fontType : fontsToLoad) {
        const auto fontFile = locateFontFile(fontType);

        if (fontFile.isEmpty()) {
            qWarning() << "Unable to find a system font";
            continue;
        }

        msdfgen::FontHandle *font = msdfgen::loadFont(freetype, fontFile.toLocal8Bit().data());

        if (font == nullptr) {
            qWarning() << "Failed to load font";
            continue;
        }

        qInfo() << "Found font file: " << fontFile;

        FontGeometry fontGeometry(&m_glyphStorage);
        fontGeometry.loadCharset(font, 1.0, charset);
        msdfgen::destroyFont(font);

        m_glyphs[fontType] = std::move(fontGeometry);
    }

    msdfgen::deinitializeFreetype(freetype);

    TightAtlasPacker packer;
    packer.setDimensionsConstraint(TightAtlasPacker::DimensionsConstraint::SQUARE);
    packer.setMinimumScale(24);
    packer.setPixelRange(8.0);
    packer.setMiterLimit(8.0);
    int width = 0;
    int height = 0;

    packer.pack(m_glyphStorage.data(), m_glyphStorage.size());
    packer.getDimensions(width, height);
    ImmediateAtlasGenerator<float, 1, sdfGenerator, BitmapAtlasStorage<byte, 1>> generator(width, height);
    generator.generate(m_glyphStorage.data(), m_glyphStorage.size());
    msdfgen::BitmapConstRef<byte, 1> atlas = generator.atlasStorage();
    QImage image(atlas.pixels, atlas.width, atlas.height, atlas.width * 1, QImage::Format_Grayscale8);

    // msdf-atlas-gen defines y-axis up. We'll flip it here to make it easier to debug
    m_image = image.mirrored(false, true);
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
    float left = 0;
    QList<FontImage::Glyph> glyphs;

    if (!m_glyphs.contains(type)) {
        return {};
    }

    const FontGeometry &fontGeometry = m_glyphs[type];

    for (QString::const_iterator it = text.cbegin(); it != text.cend(); ++it) {
        const GlyphGeometry *glyph = fontGeometry.getGlyph(it->unicode());

        if (!glyph) {
            continue;
        }

        const GlyphBox glyphBox = static_cast<GlyphBox>(*glyph);

        QPointF topLeft(left + glyphBox.bounds.l * pointSize, -glyphBox.bounds.t * pointSize);
        QPointF bottomRight = QPointF(left + glyphBox.bounds.r * pointSize, -glyphBox.bounds.b * pointSize);
        QRectF textureCoords(static_cast<float>(glyphBox.rect.x) / m_image.width(),
                             1 - static_cast<float>(glyphBox.rect.y + glyphBox.rect.h) / m_image.height(),
                             static_cast<float>(glyphBox.rect.w) / m_image.width(),
                             static_cast<float>(glyphBox.rect.h) / m_image.height());

        Glyph outputGlyph {
            textureCoords,
            QRectF(topLeft, bottomRight)
        };

        glyphs.append(outputGlyph);

        msdfgen::GlyphIndex nextGlyphIndex;
        QString::const_iterator nextIt = it + 1;
        if (nextIt != text.cend()) {
            const GlyphGeometry *nextGlyphGeometry = fontGeometry.getGlyph(nextIt->unicode());
            if (nextGlyphGeometry) {
                nextGlyphIndex = nextGlyphGeometry->getGlyphIndex();
            }
        }

        double advance = 0;
        if (fontGeometry.getAdvance(advance, glyph->getGlyphIndex(), nextGlyphIndex)) {
            left += advance * pointSize;
        }
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
