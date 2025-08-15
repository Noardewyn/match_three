#include "board.h"

#include <algorithm>
#include <cassert>
#include <vector>

Board::Board()
    : cells_(kWidth * kHeight, CellType::Red)
{
}

void Board::GenerateInitial(uint32_t seed)
{
    rng_.seed(seed);

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
    std::swap(cells_[Index(a)], cells_[Index(b)]);
}

bool Board::FindMatches(std::vector<bool> & out_mask, int & out_groups, int & out_cells) const
{
    out_mask.assign(kWidth * kHeight, false);
    out_groups = 0;
    out_cells = 0;
    return FindMatchesMask(out_mask, out_groups, out_cells);
}

int Board::CollapseAndRefillPlanned(const std::vector<bool> & mask,
                                    std::vector<Move> & out_moves,
                                    std::vector<Spawn> & out_spawns)
{
    out_moves.clear();
    out_spawns.clear();

    int removed = 0;
    for (int x = 0; x < kWidth; ++x)
    {
        int write_y = kHeight - 1;

        // Move survivors down, recording moves.
        for (int y = kHeight - 1; y >= 0; --y)
        {
            const int idx = Index({x, y});
            if (!mask[idx])
            {
                if (write_y != y)
                {
                    out_moves.push_back(Move{ IVec2{x, y}, IVec2{x, write_y} });
                }
                cells_[Index({x, write_y})] = cells_[idx];
                --write_y;
            }
            else
            {
                ++removed;
            }
        }

        // Spawn new candies at the top.
        int spawn_order = 0;
        for (int y = write_y; y >= 0; --y)
        {
            CellType t = RandomCandy();
            cells_[Index({x, y})] = t;
            out_spawns.push_back(Spawn{ IVec2{x, y}, t, spawn_order++ });
        }
    }

    return removed;
}

std::optional<std::pair<IVec2, IVec2>> Board::FindAnySwap() const
{
    std::vector<bool> mask;
    int groups = 0;
    int cells = 0;

    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            IVec2 a{x, y};

            IVec2 right{x + 1, y};
            if (InBounds(right))
            {
                Board tmp = *this;
                tmp.Swap(a, right);
                if (tmp.FindMatches(mask, groups, cells))
                {
                    return std::make_pair(a, right);
                }
            }

            IVec2 down{x, y + 1};
            if (InBounds(down))
            {
                Board tmp = *this;
                tmp.Swap(a, down);
                if (tmp.FindMatches(mask, groups, cells))
                {
                    return std::make_pair(a, down);
                }
            }
        }
    }

    return std::nullopt;
}

int Board::Index(const IVec2 & p) const
{
    return p.y * kWidth + p.x;
}

bool Board::FindMatchesMask(std::vector<bool> & out_mask, int & out_groups, int & out_cells) const
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
                    ++out_groups;
                    for (int k = 0; k < run_len; ++k)
                    {
                        const int idx = Index({x - 1 - k, y});
                        if (!out_mask[idx])
                        {
                            out_mask[idx] = true;
                            ++out_cells;
                        }
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
                    ++out_groups;
                    for (int k = 0; k < run_len; ++k)
                    {
                        const int idx = Index({x, y - 1 - k});
                        if (!out_mask[idx])
                        {
                            out_mask[idx] = true;
                            ++out_cells;
                        }
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

CellType Board::RandomCandy()
{
    std::uniform_int_distribution<int> dist(0, static_cast<int>(CellType::Count) - 1);
    return static_cast<CellType>(dist(rng_));
}

CellType Board::RandomCandyAvoiding(int x, int y)
{
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
            return c;
        }
    }

    return RandomCandy();
}
