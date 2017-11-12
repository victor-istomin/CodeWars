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
        const int    MAX_DEFEND_TICK   = 4000;
        const double MIN_HEALTH_FACTOR = 0.03;

        struct DoNothing { bool operator()() { return true; } };

        bool abortCheck();
        bool hasActionPoint()               { return state().player()->getRemainingActionCooldownTicks() == 0; }

        const double m_helicopterIteration;  // size of movement emulation increment

        bool doAttack(Callback shouldAbort, Callback shouldProceed, const VehicleGroup& attackWith, const VehicleGroup& attackTarget);

    public:
        DefendHelicopters(State& state);
    };
}

