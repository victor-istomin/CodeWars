#pragma once
#include "goal.h"
#include "forwardDeclarations.h"

namespace goals
{
    class GoalDefendTank : public Goal
    {
        static const int MAX_DEFEND_TICK            = 10000;
        static const int MAX_RESOLVE_CONFLICT_TICKS = 30;
        static const int MAX_HEAL_TICKS             = 1000;

        const double MIN_HEALTH_FACTOR = 0.03;


        const double m_helicopterIteration;  // size of movement emulation increment
        const double m_maxAgressiveDistance;
        int          m_lastConflictTick = 0;
        int          m_lastAttackTick   = 0;

        bool abortCheck() const;
        bool isHelicoptersBeaten() const;
        
        bool resolveFightersHelicoptersConflict();
        bool shiftAircraft();
        bool moveHelicopters();
        bool startFightersAttack();

        bool loopFithersAttack();

        virtual bool isCompatibleWith(const Goal* interrupted) override;

    public:
        GoalDefendTank(State& state, GoalManager& goalManager);
        ~GoalDefendTank();
    };
}

