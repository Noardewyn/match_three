#pragma once
#include "types.h"
#include <vector>
#include <random>

class Board
{
public:
    static constexpr int kWidth = 6;
    static constexpr int kHeight = 6;

    Board();

    void Randomize(uint32_t seed);
    bool InBounds(const IVec2 & p) const;
    CellType Get(const IVec2 & p) const;
    void Set(const IVec2 & p, CellType c);
    bool AreAdjacent(const IVec2 & a, const IVec2 & b) const;
    void Swap(const IVec2 & a, const IVec2 & b);

private:
    std::vector<CellType> cells_;

    int Index(const IVec2 & p) const;
};
