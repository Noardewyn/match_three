#include "board.h"

#include <algorithm>
#include <cassert>

Board::Board()
    : cells_(kWidth * kHeight, CellType::Red)
{
}

void Board::GenerateInitial(uint32_t seed)
{
    rng_.seed(seed);

    // Fill grid ensuring no immediate 3-in-a-row or 3-in-a-column
    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            cells_[Index({x, y})] = RandomCandyAvoiding(x, y);
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
    if (!InBounds(a) || !InBounds(b))
    {
        return;
    }
    const int ia = Index(a);
    const int ib = Index(b);
    std::swap(cells_[ia], cells_[ib]);
}

bool Board::TrySwapAndResolve(const IVec2 & a, const IVec2 & b)
{
    if (!InBounds(a) || !InBounds(b) || !AreAdjacent(a, b))
    {
        return false;
    }

    // Perform tentative swap.
    Swap(a, b);

    // Look for matches after swap.
    std::vector<bool> mask(kWidth * kHeight, false);
    if (!FindMatchesMask(mask))
    {
        // No matches – revert swap.
        Swap(a, b);
        return false;
    }

    // Resolve cascades until there are no more matches.
    // Safety cap prevents infinite loops in case of a bug.
    int safety_cap = 64;
    do
    {
        CollapseColumnsAndRefill(mask);
        mask.assign(kWidth * kHeight, false);
    }
    while (--safety_cap > 0 && FindMatchesMask(mask));

    return true;
}

int Board::Index(const IVec2 & p) const
{
    return p.y * kWidth + p.x;
}

bool Board::FindMatchesMask(std::vector<bool> & out_mask) const
{
    bool any = false;

    // Horizontal runs
    for (int y = 0; y < kHeight; ++y)
    {
        int run_len = 1;
        for (int x = 1; x <= kWidth; ++x)
        {
            bool same = false;
            if (x < kWidth)
            {
                same = (Get({x, y}) == Get({x - 1, y}));
            }
            if (!same)
            {
                if (run_len >= 3)
                {
                    any = true;
                    for (int k = 0; k < run_len; ++k)
                    {
                        out_mask[Index({x - 1 - k, y})] = true;
                    }
                }
                run_len = 1;
            }
            else
            {
                ++run_len;
            }
        }
    }

    // Vertical runs
    for (int x = 0; x < kWidth; ++x)
    {
        int run_len = 1;
        for (int y = 1; y <= kHeight; ++y)
        {
            bool same = false;
            if (y < kHeight)
            {
                same = (Get({x, y}) == Get({x, y - 1}));
            }
            if (!same)
            {
                if (run_len >= 3)
                {
                    any = true;
                    for (int k = 0; k < run_len; ++k)
                    {
                        out_mask[Index({x, y - 1 - k})] = true;
                    }
                }
                run_len = 1;
            }
            else
            {
                ++run_len;
            }
        }
    }

    return any;
}

int Board::CollapseColumnsAndRefill(const std::vector<bool> & mask)
{
    int removed = 0;
    for (int x = 0; x < kWidth; ++x)
    {
        // Move non-removed cells down.
        int write_y = kHeight - 1;
        for (int y = kHeight - 1; y >= 0; --y)
        {
            const int idx = Index({x, y});
            if (!mask[idx])
            {
                cells_[Index({x, write_y})] = cells_[idx];
                --write_y;
            }
            else
            {
                ++removed;
            }
        }

        // Fill the top part with new random candies.
        for (int y = write_y; y >= 0; --y)
        {
            // Here we could use RandomCandyAvoiding to reduce immediate matches,
            // but allowing them creates natural cascades which is desired.
            cells_[Index({x, y})] = RandomCandy();
        }
    }
    return removed;
}

CellType Board::RandomCandy()
{
    std::uniform_int_distribution<int> dist(0, static_cast<int>(CellType::Count) - 1);
    return static_cast<CellType>(dist(rng_));
}

CellType Board::RandomCandyAvoiding(int x, int y)
{
    // Try to avoid creating immediate 3-in-a-row while building the initial grid.
    // We will try a few random candidates; if all fail, fallback to the last one.
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        CellType c = RandomCandy();

        bool bad = false;
        if (x >= 2)
        {
            const CellType c1 = Get({x - 1, y});
            const CellType c2 = Get({x - 2, y});
            if (c == c1 && c == c2) bad = true;
        }
        if (!bad && y >= 2)
        {
            const CellType c1 = Get({x, y - 1});
            const CellType c2 = Get({x, y - 2});
            if (c == c1 && c == c2) bad = true;
        }

        if (!bad)
        {
            return c;
        }
        if (attempt == 7)
        {
            return c; // give up after several tries
        }
    }

    return RandomCandy();
}
