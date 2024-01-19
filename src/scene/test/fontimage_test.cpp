#include <cmath>
#include <gtest/gtest.h>

#include <scene/fontimage.h>

TEST(FontImageTest, SaveAtlasToFile)
{
    FontImage fontImage;
    const QImage &image = fontImage.image();
    image.save("atlas.png");
}
