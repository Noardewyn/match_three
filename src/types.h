#pragma once
#include <cstdint>

struct IVec2
{
    int x {0};
    int y {0};
};

enum class CellType : uint8_t
{
    Red = 0,
    Green,
    Blue,
    Yellow,
    Purple,
    Orange,
    Count
};
