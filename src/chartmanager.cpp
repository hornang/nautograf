#include <memory>

#include "ui/chartmanager.h"

using string = std::string;

void ChartManager::loadChart(const string &chartFile, Type type)
{
    switch (type) {
    case Type::Oesenc:
        loadOesenc(chartFile);
        break;
    default:
        return;
    }
}

void ChartManager::loadOesenc(const string &chartFile)
{
    m_chart = std::make_unique<ChartFile>();
    m_chart->read(chartFile);
}
