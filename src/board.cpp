#include "board.h"

Board::Board()
    : cells_(kWidth * kHeight, CellType::Red)
{
}

void Board::Randomize(uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(CellType::Count) - 1);

    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            cells_[Index({x, y})] = static_cast<CellType>(dist(rng));
        }
    }
}

bool Board::InBounds(const IVec2 & p) const
{
    return p.x >= 0 && p.y >= 0 && p.x < kWidth && p.y < kHeight;
}

CellType Board::Get(const IVec2 & p) const
{
    return cells_[Index(p)];
}

void Board::Set(const IVec2 & p, CellType c)
{
    cells_[Index(p)] = c;
}

bool Board::AreAdjacent(const IVec2 & a, const IVec2 & b) const
{
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    return (dx + dy) == 1;
}

void Board::Swap(const IVec2 & a, const IVec2 & b)
{
    if (!InBounds(a) || !InBounds(b) || !AreAdjacent(a, b))
    {
        return;
    }
    const int ia = Index(a);
    const int ib = Index(b);
    std::swap(cells_[ia], cells_[ib]);
}

int Board::Index(const IVec2 & p) const
{
    return p.y * kWidth + p.x;
}
