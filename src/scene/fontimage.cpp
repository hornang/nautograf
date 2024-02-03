#include <QCoreApplication>
#include <QCryptographicHash>
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
using namespace std;

namespace {

string getFontName(FontImage::FontType type)
{
    switch (type) {
    case FontImage::FontType::Normal:
        return "normal";
    case FontImage::FontType::Soundings:
        return "soundings";
    }
    return {};
}

FontImage::FontType getFontType(const string &name)
{
    if (name == "normal") {
        return FontImage::FontType::Normal;
    } else if (name == "soundings") {
        return FontImage::FontType::Soundings;
    }
    return FontImage::FontType::Unknown;
}

QString getCacheDir()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (cacheDir.isEmpty()) {
        return {};
    }

    QByteArray hashOfAppVersion = QCryptographicHash::hash(QByteArray(APP_VERSION), QCryptographicHash::Md5);

    return cacheDir
        + QDir::separator() + "glyphs" + QDir::separator() + hashOfAppVersion.toHex() + QDir::separator();
}

bool generateAtlas(const string &pngFileName, const string &layoutFileName)
{
    msdfgen::FreetypeHandle *freetype = msdfgen::initializeFreetype();

    if (freetype == nullptr) {
        qWarning() << "Unable to initialize freetype";
        return false;
    }

    vector<GlyphGeometry> glyphStorage;
    vector<FontGeometry> fontGeometries;

    const QString glyphsToRender(R"(ABCDEFGHIJKLMNOPQRSTUVWXYZÆØÅabcdefghijklmnopqrstuvwxyz1234567890æøå.,-? )");

    Charset charset;

    for (const QChar &c : glyphsToRender) {
        charset.add(c.unicode());
    }

    for (const auto &fontType : { FontImage::FontType::Normal,
                                  FontImage::FontType::Soundings }) {
        const auto fontFile = FontImage::locateFontFile(fontType);

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

        FontGeometry fontGeometry(&glyphStorage);
        fontGeometry.setName(getFontName(fontType).data());
        fontGeometry.loadCharset(font, 1.0, charset);
        msdfgen::destroyFont(font);

        fontGeometries.push_back(fontGeometry);
    }

    msdfgen::deinitializeFreetype(freetype);

    TightAtlasPacker packer;
    packer.setDimensionsConstraint(TightAtlasPacker::DimensionsConstraint::SQUARE);
    packer.setMinimumScale(24);
    packer.setPixelRange(8.0);
    packer.setMiterLimit(8.0);
    int width = 0;
    int height = 0;

    packer.pack(glyphStorage.data(), glyphStorage.size());
    packer.getDimensions(width, height);
    ImmediateAtlasGenerator<float, 1, sdfGenerator, BitmapAtlasStorage<msdfgen::byte, 1>> generator(width, height);
    generator.generate(glyphStorage.data(), glyphStorage.size());
    msdfgen::BitmapConstRef<msdfgen::byte, 1> atlas = generator.atlasStorage();

    if (!saveImage(static_cast<msdfgen::BitmapConstRef<msdfgen::byte, 1>>(atlas),
                   ImageFormat::PNG,
                   pngFileName.data(),
                   YDirection::TOP_DOWN)) {
        qWarning() << "Failed to save atlas image";
        return false;
    }

    if (!exportJSON(fontGeometries.data(), fontGeometries.size(), 12, 8,
                    atlas.width, atlas.height,
                    ImageType::SDF, YDirection::TOP_DOWN, layoutFileName.data(), true)) {
        qWarning() << "Failed to save atlas layout";
        return false;
    }

    return true;
}

unordered_map<FontImage::FontType, msdf_atlas_read::FontGeometry> getGeometriesByFontType(const msdf_atlas_read::AtlasLayout &atlas)
{
    unordered_map<FontImage::FontType, msdf_atlas_read::FontGeometry> geometriesByType;

    for (const msdf_atlas_read::FontGeometry &fontGeometry : atlas.fontGeometries) {
        FontImage::FontType type = getFontType(fontGeometry.name);

        if (type == FontImage::FontType::Unknown) {
            continue;
        }

        geometriesByType[type] = fontGeometry;
    }

    return geometriesByType;
}
}

FontImage::FontImage()
{
    QDir cacheDir = getCacheDir();

    if (!loadCache(cacheDir)) {
        if (createCache(cacheDir) && loadCache(cacheDir)) {
            qWarning() << "Failed to generate font atlas. Cannot display text in map";
        }
    }
}

const char *glyphImageFileName = "glyphs.png";
const char *glyphLayoutFileName = "layout.json";

bool FontImage::createCache(const QDir &cacheDir)
{
    if (!cacheDir.mkpath(".")) {
        return false;
    }

    return generateAtlas(cacheDir.filePath(glyphImageFileName).toStdString(),
                         cacheDir.filePath(glyphLayoutFileName).toStdString());
}

bool FontImage::loadCache(const QDir &cacheDir)
{
    if (!cacheDir.exists()) {
        return false;
    }

    try {
        msdf_atlas_read::AtlasLayout atlas = msdf_atlas_read::loadJson(cacheDir.filePath(glyphLayoutFileName).toStdString());

        m_fontGeometries = getGeometriesByFontType(atlas);

        if (!m_image.load(cacheDir.filePath(glyphImageFileName))) {
            return false;
        }
    } catch (std::exception &e) {
        qWarning() << "Exception catched: " << e.what();
        return false;
    }

    return true;
}

QRectF FontImage::boundingBox(const QString &text, float pointSize, FontType type) const
{
    int yMin = std::numeric_limits<int>::max();
    int yMax = std::numeric_limits<int>::min();
    int xMax = std::numeric_limits<int>::min();

    auto glyphs = FontImage::glyphs(text, pointSize, type);

    for (const msdf_atlas_read::GlyphMapping &mapping : glyphs) {
        yMin = min<float>(mapping.targetBounds.top, yMin);
        yMax = max<float>(mapping.targetBounds.bottom, yMax);
        xMax = max<float>(mapping.targetBounds.right, xMax);
    }

    return QRectF(0, yMin, xMax, yMax - yMin);
}

QList<msdf_atlas_read::GlyphMapping> FontImage::glyphs(const QString &text,
                                                       float pointSize,
                                                       FontType type) const
{

    float left = 0;
    QList<msdf_atlas_read::GlyphMapping> glyphs;

    if (!m_fontGeometries.contains(type)) {
        return {};
    }

    const msdf_atlas_read::FontGeometry &fontGeometry = m_fontGeometries.at(type);

    for (QString::const_iterator it = text.cbegin(); it != text.cend(); ++it) {

        const msdf_atlas_read::Glyph *glyph = fontGeometry.getGlyph(it->unicode());

        if (!glyph) {
            continue;
        }

        if (glyph->hasMapping()) {
            const msdf_atlas_read::GlyphMapping &glyphBox = glyph->getMapping();

            msdf_atlas_read::GlyphMapping glyphMapping = glyph->getMapping();
            glyphMapping.scale(pointSize);
            glyphMapping.moveRight(left);
            glyphs.append(glyphMapping);
        }

        left += glyph->advance * pointSize;

        QString::const_iterator nextIt = it + 1;
        if (nextIt != text.cend()) {
            if (fontGeometry.kerning.contains(it->unicode())) {
                const auto &kerningMap = fontGeometry.kerning.at(it->unicode());
                if (kerningMap.contains(nextIt->unicode())) {
                    float advance = kerningMap.at(nextIt->unicode());
                    left += advance * pointSize;
                }
            }
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
    case FontImage::FontType::Normal:
        fonts = dir.entryList({ "arial*" });
        break;
    case FontImage::FontType::Soundings:
        fonts = dir.entryList({ "ariali*" });
        break;
    }
#endif
    if (fonts.isEmpty()) {
        return {};
    }

    return dir.filePath(fonts.first());
}
