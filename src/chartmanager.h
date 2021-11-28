#pragma once

#include "oesenc/chartfile.h"

class ChartManager
{
public:
    enum class Type {
        Oesenc = 0;
    };

    ChartManager();
    void loadChart(const std::string &chartFile, Type type);

private:
    void loadOesenc(const string &chartFile);
    std::unique_ptr<ChartFile> m_chart;
};
