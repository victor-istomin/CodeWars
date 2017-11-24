#pragma once
#include "goal.h"
#include "forwardDeclarations.h"
#include "model/VehicleType.h"
#include "VehicleGroup.h"

#include <map>
#include <list>

namespace goals
{
    template <typename Functor> class And
    {
        std::list<Functor> m_array;

    public:

        template <typename FunctorArray, size_t size>
        And(FunctorArray const (&functors)[size])
        {
            for (auto& a : functors)
                m_array.emplace_back(a);
        }

        bool operator()()
        {
            for (auto& item : m_array)
                if (!item())
                    return false;

            return true;
        }
    };

    template <typename FunctorArray, size_t size>
    And<FunctorArray> makeAnd(FunctorArray const (&functors)[size])
    { 
        return And<FunctorArray>(functors);
    }

    class MixTanksAndHealers : public Goal
    {
        struct GridPos
        {
            int m_x;
            int m_y;
            GridPos(int x, int y) : m_x(x), m_y(y)      {}
            GridPos()             : m_x(0), m_y(0)      {}
            bool operator==(const GridPos& other) const { return m_x == other.m_x && m_y == other.m_y; }
        };

        typedef std::map<model::VehicleType, GridPos>       PosByType;
        typedef std::list<Point>                            Destinations;
        typedef std::map<model::VehicleType, Destinations>  MovePlan;

        static const model::VehicleType s_groundUnits[];

        double m_iterationSize;
		Point m_topLeftMargin;
		Point m_gridCellSize;
        MovePlan m_overallMoves;
        MovePlan m_pendingMoves;

		std::map<int, double> m_xGridToPos;   // convert column number to world position
		std::map<int, double> m_yGridToPos;   // convert row number to world position

        Destinations getMoves(model::VehicleType groupType, const Point& actualCenter, const GridPos& actual, const GridPos& destination, bool allowShifting);
        static double getPathLength(const Point& start, const Destinations& path);

		void  initGridPositions();

		GridPos pointToPos(const Point& center);
		Point   posToPoint(const GridPos& pos);

		void setupGoalSteps();

        bool applyMovePlan();
        bool scaleGroups();
        bool revertScale();
        bool mixGroups();

    public:
        explicit MixTanksAndHealers(State& state);

        void getGroundUnitOrder(PosByType& actualPositions, PosByType& desiredPositions);

        Point getFinalDestination(model::VehicleType type) const;

        ~MixTanksAndHealers();
    };

}

