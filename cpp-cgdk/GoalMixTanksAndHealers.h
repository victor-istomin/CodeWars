#pragma once
#include "goal.h"
#include "forwardDeclarations.h"
#include "model/VehicleType.h"
#include "VehicleGroup.h"

#include <map>
#include <list>

namespace goals
{
    class MixTanksAndHealers : public Goal
    {
        struct GridPos
        {
            int m_x;
            int m_y;
            GridPos(int x, int y) : m_x(x), m_y(y)      {}
            bool operator==(const GridPos& other) const { return m_x == other.m_x && m_y == other.m_y; }

        };

        typedef std::map<model::VehicleType, GridPos>       PosByType;
        typedef std::list<Point>                            Destinations;
        typedef std::map<model::VehicleType, Destinations>  MovePlan;

        static const model::VehicleType s_groundUnits[];

        double m_iterationSize;

        Destinations getMoves(model::VehicleType groupType, const Point& actualCenter, const GridPos& actual, const GridPos& destination);
        static double getPathLength(const Point& start, const Destinations& path);


        bool shiftIfv();

        struct NeverAbort { bool operator()() { return false; } };

    public:
        MixTanksAndHealers(State& state);

        void getGroundUnitOrder(PosByType& actualPositions, PosByType& desiredPositions);

        ~MixTanksAndHealers();
    };

}

