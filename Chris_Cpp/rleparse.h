#pragma once

#include <vector>

struct AliveDead
{
    int count;
    bool alive;
};

struct RLE
{
    int x;
    int y;
    std::vector<std::vector<AliveDead>> runs;
};

std::vector<RLE> GatherPatterns();
