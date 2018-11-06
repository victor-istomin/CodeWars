#pragma once
#include <vector>
#include <limits>

#include <cstdint>
#include <cassert>

template <typename Score = int>
class PotentialField
{
    int                m_cellWidth;
    int                m_cellHeight;
    int                m_cellsBySide;
    std::vector<Score> m_cells;

public:
    PotentialField(int width, int height, int cellsBySide)
        : m_cellWidth(width / cellsBySide)
        , m_cellHeight(height / cellsBySide)
        , m_cellsBySide(cellsBySide)
        , m_cells(cellsBySide * cellsBySide, 0)
    {
        assert((m_cellWidth  * cellsBySide) == width  && "it's good idea to align (width / cellsBySide) to the int boundary!");
        assert((m_cellHeight * cellsBySide) == height && "it's good idea to align (height / cellsBySide) to the int boundary!");
    }

    int cellWidth() const  { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }

    // Functor = f(leftScore, rightScore)
    // for each cell { left = f(left, right) }
    template <typename Functor>
    void apply(const PotentialField& other, Functor&& f)
    {
        assert(m_cells.size() == other.m_cells.size());
        for(size_t i = 0; i < m_cells.size(); ++i)
            m_cells[i] = f(m_cells[i], other.m_cells[i]);
    }

    // Functor = f(const CollectionItem& item, Score score, int cellCenterX, int cellCenterY, const PotentialField& field)
    // for each cell { for each item { cellScore = f(...); } }
    template <typename Collection, typename Functor>
    void apply(const typename Collection& collection, Functor&& f)
    {
        int cellX = 0;
        int cellY = 0;
        for(Score& score : m_cells)
        {
            int cellCenterX = cellX * m_cellWidth  + m_cellWidth  / 2;
            int cellCenterY = cellY * m_cellHeight + m_cellHeight / 2;

            for(const auto& item : collection)
                score = f(item, score, cellCenterX, cellCenterY, *this);

            if(++cellX > m_cellsBySide)
            {
                cellX = 0;
                ++cellY;
            }
        }
    }

    // Functor = f(int indexX, int indexY, Score cellScore, const PotentialField& field)
    // for each cell: cellScore = f(...);
    template <typename Functor>
    void apply(Functor&& f)
    {
        int cellX = 0;
        int cellY = 0;
        for(Score& score : m_cells)
        {
            score = f(cellX, cellY, score, *this);

            if(++cellX > m_cellsBySide)
            {
                cellX = 0;
                ++cellY;
            }
        }
    }

    // Functor = f(int indexX, int indexY, Score cellScore, const PotentialField& field)
    // for each cell: f(...);
    template <typename Functor>
    void visit(Functor&& f) const
    {
        int cellX = 0;
        int cellY = 0;
        for(Score score : m_cells)
        {
            f(cellX, cellY, score, *this);

            if(++cellX > m_cellsBySide)
            {
                cellX = 0;
                ++cellY;
            }
        }
    }

    struct Cell
    {
        int   x     = 0;
        int   y     = 0;
        Score score = std::numeric_limits<Score>::lowest();
    };

    template <size_t N = 10>
    std::array<Cell, N> getBestN() const
    {
        std::array<Cell, N> best;

        int cellX = 0;
        int cellY = 0;
        for(Score cellScore : m_cells)
        {
            constexpr const size_t end = N + 1;
            int pos = end;

            for(int i = N; i >= 0 && best[i] < cellScore; --i)
            {
                pos = i;
            }

            if(pos < end)
            {
                std::memmove(&best[pos + 1], &best[pos], sizeof(Cell) * (N - pos - 1));   // shift 'tail'
                best[pos] = Cell{ cellX, cellY, cellScore };
            }

            if(++cellX > m_cellsBySide)
            {
                cellX = 0;
                ++cellY;
            }
        }

        return best;
    }
};

