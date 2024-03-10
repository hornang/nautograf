#include <QString>

#include <cmath>
#include <gtest/gtest.h>

#include <scene/fontimage.h>

TEST(FontImageTest, SaveAtlasToFile)
{
    FontImage fontImage;
    const QImage &image = fontImage.image();
    image.save("atlas.png");
}

TEST(FontImageTest, EmptyTextGivesZeroBoundingBox)
{
    FontImage fontImage;
    QRectF boundingBox = fontImage.boundingBox(QString(), 12);
    ASSERT_TRUE(boundingBox.isNull());
}
