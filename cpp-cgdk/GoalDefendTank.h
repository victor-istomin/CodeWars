#pragma once
#include "goal.h"

namespace goals
{
    class GoalDefendTank : public Goal
    {
        static const int MAX_DEFEND_TICK = 10000;
        static const int MAX_RESOLVE_CONFLICT_TICKS = 30;
        
        const double MIN_HEALTH_FACTOR = 0.03;


        const double m_helicopterIteration;  // size of movement emulation increment
        int          m_lastConflictTick = 0;


        bool abortCheck() const;
        
        bool resolveFightersHelicoptersConflict();
        bool shiftAircraft();
        bool moveHelicopters();
        bool startFightersAttack();

        bool loopFithersAttack();

    public:
        GoalDefendTank(State& state);
        ~GoalDefendTank();
    };
}

