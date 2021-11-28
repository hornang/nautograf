#include "tilefactorywrapper.h"

std::vector<std::shared_ptr<Chart>> TileFactoryWrapper::create(TileRecipe recipe)
{
    return m_tileDataCallback(recipe.rect, recipe.pixelsPerLongitude);
}

void TileFactoryWrapper::setTileDataCallback(TileDataCallback tileDataCallback)
{
    m_tileDataCallback = tileDataCallback;
}
