#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QRectF>
#include <QStandardPaths>
#include <QString>
#include <QThread>

#ifdef Q_OS_LINUX
#include <fontconfig/fontconfig.h>
#endif

#include "scene/annotations/fontimage.h"

using namespace msdf_atlas;
using namespace std;

namespace {

Charset createLatinCharset()
{
    Charset charset;

    for (unicode_t codepoint = 0x0020; codepoint < 0x007f; codepoint++) {
        charset.add(codepoint);
    }

    for (unicode_t codepoint = 0x00A0; codepoint < 0x00ff; codepoint++) {
        charset.add(codepoint);
    }

    return charset;
}

string getFontName(FontType type)
{
    switch (type) {
    case FontType::Normal:
        return "normal";
    case FontType::Soundings:
        return "soundings";
    }
    return {};
}

FontType getFontType(const string &name)
{
    if (name == "normal") {
        return FontType::Normal;
    } else if (name == "soundings") {
        return FontType::Soundings;
    }
    return FontType::Unknown;
}

QString getAtlasDir()
{
    QString atlasDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (atlasDir.isEmpty()) {
        return {};
    }

    QByteArray hashOfAppVersion = QCryptographicHash::hash(QByteArray(APP_VERSION), QCryptographicHash::Md5);

    return atlasDir
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

    Charset charset = createLatinCharset();

    for (const auto &fontType : { FontType::Normal,
                                  FontType::Soundings }) {
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

unordered_map<FontType, msdf_atlas_read::FontGeometry> getGeometriesByFontType(const msdf_atlas_read::AtlasLayout &atlas)
{
    unordered_map<FontType, msdf_atlas_read::FontGeometry> geometriesByType;

    for (const msdf_atlas_read::FontGeometry &fontGeometry : atlas.fontGeometries) {
        FontType type = getFontType(fontGeometry.name);

        if (type == FontType::Unknown) {
            continue;
        }

        geometriesByType[type] = fontGeometry;
    }

    return geometriesByType;
}

const char *glyphImageFileName = "glyphs.png";
const char *glyphLayoutFileName = "layout.json";

bool createCache(const QDir &cacheDir)
{
    if (!cacheDir.mkpath(".")) {
        return false;
    }

    return generateAtlas(cacheDir.filePath(glyphImageFileName).toStdString(),
                         cacheDir.filePath(glyphLayoutFileName).toStdString());
}

std::optional<FontImage::Atlas> getAtlas(const QDir &cacheDir)
{
    if (!cacheDir.exists()) {
        createCache(cacheDir);
    }

    msdf_atlas_read::AtlasLayout atlasLayout;

    try {
        atlasLayout = msdf_atlas_read::loadJson(cacheDir.filePath(glyphLayoutFileName).toStdString());
    } catch (const std::exception &e) {
        qWarning() << "Failed to load";
        return {};
    }

    QImage image;
    if (!image.load(cacheDir.filePath(glyphImageFileName))) {
        return {};
    }
    return FontImage::Atlas { image, getGeometriesByFontType(atlasLayout) };
}
}

class FontWorker : public QThread
{
    Q_OBJECT

    void run() override
    {
        QDir cacheDir = getAtlasDir();
        std::optional<FontImage::Atlas> cache = getAtlas(cacheDir);
        if (!cache.has_value()) {
            qWarning() << "Failed to generate font atlas. Cannot display text in map";
            return;
        }
        emit atlasUpdated(cache.value());
    }

signals:
    void atlasUpdated(const FontImage::Atlas &cache);
};

#include "fontimage.moc"

FontImage::FontImage()
{
    m_worker = new FontWorker();
    connect(m_worker, &FontWorker::atlasUpdated, this, &FontImage::setAtlas);
    m_worker->start();
}

FontImage::~FontImage()
{
    m_worker->wait();
    delete m_worker;
}

void FontImage::setAtlas(const FontImage::Atlas &atlas)
{
    m_atlas = atlas;
    emit atlasChanged();
}

QRectF FontImage::boundingBox(const QString &text, float pointSize, FontType type) const
{
    if (text.isEmpty()) {
        return {};
    }

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
    if (!m_atlas.has_value()) {
        return {};
    }

    float left = 0;
    QList<msdf_atlas_read::GlyphMapping> glyphs;

    if (!m_atlas->geometries.contains(type)) {
        return {};
    }

    const msdf_atlas_read::FontGeometry &fontGeometry = m_atlas->geometries.at(type);

    for (QString::const_iterator it = text.cbegin(); it != text.cend(); ++it) {

        const msdf_atlas_read::Glyph *glyph = fontGeometry.getGlyph(it->unicode());

        if (!glyph) {
            QChar space(QChar::Space);
            glyph = fontGeometry.getGlyph(space.unicode());

            if (!glyph) {
                continue;
            }
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
    FcPattern *pattern = FcPatternCreate();
    if (!pattern) {
        return {};
    }

    int slant = FC_SLANT_ROMAN;

    if (type == FontType::Soundings) {
        slant = FC_SLANT_ITALIC;
    }

    static const char *fontFamily = "Ubuntu";

    FcPatternAddInteger(pattern, FC_SLANT, slant);
    FcPatternAddString(pattern, FC_FAMILY, (FcChar8 *)fontFamily);

    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern *match = FcFontMatch(NULL, pattern, &result);

    if (match && result == FcResultMatch) {
        FcChar8 *file = nullptr;
        if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
            return QString(reinterpret_cast<char *>(file));
        }

        FcPatternDestroy(match);
    }

    FcPatternDestroy(pattern);
    return {};
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

    if (fonts.isEmpty()) {
        return {};
    }

    return dir.filePath(fonts.first());
#endif
}
