#pragma once
#include "goal.h"

namespace goals
{
    class GoalDefendIfv : public Goal
    {
        static const int MAX_DEFEND_TICK = 17000;
        static const int MAX_RESOLVE_CONFLICT_TICKS = 30;

        const double MIN_HEALTH_FACTOR = 0.02;

        const double m_helicopterIteration;  // size of movement emulation increment
        //int          m_lastConflictTick = 0;

        bool abortCheck() const;
        bool isTanksBeaten() const;

        bool shiftAircraft();
        bool moveHelicopters();


    public:
        GoalDefendIfv(State& worldState, GoalManager& goalManager);
        ~GoalDefendIfv();
    };
}

