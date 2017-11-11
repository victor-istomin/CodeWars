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
        const int    MAX_DEFEND_TICK   = 3000;
        const double MIN_HEALTH_FACTOR = 0.03;

        struct WaitSomeTicks
        {
            int m_ticksRemaining;

            bool operator()() { return m_ticksRemaining-- <= 0; }
        };

        struct DoNothing { bool operator()() { return true; } };

        const VehicleGroup& ifvGroup();
        const VehicleGroup& tankGroup();
        const VehicleGroup& helicopterGroup();
        const VehicleGroup& fighterGroup();
        const VehicleGroup& allienFighters();
		const VehicleGroup& allienHelicopters();

        static bool canMoveRectTo(const Point& from, const Point& to, Rect fromRect, Rect obstacleRect, double iterationSize);

        static bool isPathFree(const VehicleGroup& group, const Point& to, const VehicleGroup& obstacle, double iterationSize);

        bool abortCheck();
        bool hasActionPoint()               { return state().player()->getRemainingActionCooldownTicks() == 0; }

        const double m_helicopterIteration;  // size of movement emulation increment

        bool doAttack(Callback shouldAbort, Callback shouldProceed, const VehicleGroup& attackWith, const VehicleGroup& attackTarget);

    public:
        DefendHelicopters(State& state);
    };
}

