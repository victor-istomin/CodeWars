#pragma once
#include <vector>
#include <limits>

#include <cstdint>
#include <cassert>
#include <functional>
#include <type_traits>

struct TagIsPotentialField {};

template <typename T>
constexpr bool isPotentialField(const T&) { return false; }

template <>
constexpr bool isPotentialField(const TagIsPotentialField&) { return true; }


template <typename Score, typename Point, int CellsBySide>
class PotentialField : public TagIsPotentialField
{
public :
    struct Index
    {
        int x = 0;
        int y = 0;

        Index& operator++() 
        {
            if(++x == CellsBySide)
            {
                x = 0;
                ++y;
            }
            return *this;
        }
    };

    constexpr static int k_SideSize = CellsBySide;

    int                m_cellWidth;
    int                m_cellHeight;
    Point              m_dxdy;
    Point              m_dxdyCellCenter;     // cache, used very often
    std::vector<Score> m_cells;

    static int alignSize(double size)
    {
        return static_cast<int>(std::ceil(size / CellsBySide)) * CellsBySide;
    };


public:
    PotentialField(int width, int height, const Point& dxdy)
        : m_cellWidth(alignSize(width) / CellsBySide)
        , m_cellHeight(alignSize(height) / CellsBySide)
        , m_dxdy(dxdy)
        , m_dxdyCellCenter(dxdy + Point(m_cellWidth / 2, m_cellHeight / 2))
        , m_cells(CellsBySide * CellsBySide, 0)
    {
    }

    Point cellCenterToWorld (const Index& index) const { return m_dxdyCellCenter + Point(index.x * m_cellWidth, index.y * m_cellHeight); }
    Point cellTopLeftToWorld(const Index& index) const { return m_dxdy + Point(index.x * m_cellWidth, index.y * m_cellHeight); }
    
    Index worldToCell(Point point) const 
    { 
        point -= m_dxdy; 
        return Index { static_cast<int>(point.m_x / m_cellWidth), static_cast<int>(point.m_y / m_cellHeight) };
    }

    Score get(const Index& index) const 
    { 
        return m_cells[index.x + index.y * CellsBySide];
    }

    int cellWidth() const  { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }

    // Functor = f(leftScore, rightScore)
    // for each cell { left = f(left, right) }
    template <typename OtherField, typename Functor>
    auto apply(const OtherField& other, Functor&& f) -> std::enable_if_t<std::is_base_of_v<TagIsPotentialField, OtherField>>
    {
        static_assert(k_SideSize == OtherField::k_SideSize, "field dimensions should match");

        assert(m_cells.size() == other.m_cells.size());
        for(size_t i = 0; i < m_cells.size(); ++i)
            m_cells[i] = f(m_cells[i], other.m_cells[i]);
    }

    // Functor = f(const CollectionItem& item, Score score, Point cellCenter, const PotentialField& field)
    // for each cell { for each item { cellScore = f(...); } }
    template <typename Collection, typename Functor>
    auto apply(const typename Collection& collection, Functor&& f) -> std::enable_if_t<!std::is_base_of_v<TagIsPotentialField, Collection>>
    {
        Index index;
        for(Score& score : m_cells)
        {
            for(const auto& item : collection)
                score = f(item, score, cellCenterToWorld(index), *this);

            ++index;
        }
    }

//     // Functor = f(int indexX, int indexY, Score cellScore, const PotentialField& field)
//     // for each cell: cellScore = f(...);
//     template <typename Functor>
//     void apply(Functor&& f)
//     {
//         int cellX = 0;
//         int cellY = 0;
//         for(Score& score : m_cells)
//         {
//             score = f(cellX, cellY, score, *this);
// 
//             if(++cellX == CellsBySide)
//             {
//                 cellX = 0;
//                 ++cellY;
//             }
//         }
//     }

    // Functor = f(Index cellIndex, Score cellScore, const PotentialField& field)
    // for each cell: f(...);
    template <typename Functor>
    void visit(Functor&& f) const
    {
        Index index;
        for(Score score : m_cells)
        {
            f(index, score, *this);
            ++index;
        }
    }

    struct Cell   // #todo - reuse index
    {
        int   x     = 0;
        int   y     = 0;
        Score score = std::numeric_limits<Score>::lowest();
    };

    template <size_t N = 10>
    std::array<Cell, N> getBestN() const
    {
        std::array<const Score*, N> bestPointers;

        for(auto it = std::begin(cells); it != std::end(cells); ++it)
        {
            const Score& cellScore = *it;

            constexpr const size_t end = N + 1;
            int pos = end;

            for(int i = N; i >= 0 && *bestPointers[i] < cellScore; --i)
            {
                pos = i;
            }

            if(pos < end)
            {
                std::memmove(&bestPointers[pos + 1], &bestPointers[pos], sizeof(Cell) * (N - pos - 1));   // shift 'tail'
                bestPointers[pos] = &cellScore;
            }
        }

        std::array<Cell, N> best;
        for(size_t i = 0; i < bestPointers.size(); ++i)
        {
            const Score* ptr = bestPointers[i];
            ptrdiff_t pos = ptr - std::begin(m_cells);
            best[i] = Cell{ pos % CellsBySide, pos / CellsBySide, *ptr };
        }

        return best;
    }
};

