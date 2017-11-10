#pragma once
#include <memory>
#include <list>
#include "goal.h"
#include "forwardDeclarations.h"
#include "state.h"

namespace goals
{
    class DefendHelicopters : public Goal
    {
        const int MAX_DEFEND_TICK = 500;

        const VehicleGroup& ifvGroup();
        const VehicleGroup& tankGroup();
        const VehicleGroup& helicopterGroup();
        const VehicleGroup& fighterGroup();
        const VehicleGroup& allienFighters();

        static bool canMoveRectTo(const Point& from, const Point& to, Rect fromRect, Rect obstacleRect, double iterationSize);

        static bool isPathFree(const VehicleGroup& group, const Point& to, const VehicleGroup& obstacle, double iterationSize);

        bool abortCheck()                   { return state().world()->getTickIndex() > MAX_DEFEND_TICK; }
        bool hasActionPoint()               { return state().player()->getRemainingActionCooldownTicks() == 0; }

        const double m_helicopterIteration;  // size of movement emulation increment

    public:
        DefendHelicopters(State& state);
    };
}

