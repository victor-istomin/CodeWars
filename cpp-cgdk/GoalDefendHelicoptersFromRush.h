#pragma once
#include <memory>
#include <list>
#include "goal.h"
#include "forwardDeclarations.h"
#include "state.h"

namespace goals
{
    class DefendHelicoptersFromRush : public Goal
    {
        const int    MAX_DEFEND_TICK      = 8000;
        const int    DEFEND_DECISION_TICK = 500;   // QuickStart guy is near my base on this tick. However, current code needs ~200 ticks from start to detect rush
        const double MIN_HEALTH_FACTOR    = 0.03;

        struct DoNothing { bool operator()() { return true; } };

        bool abortCheck();
        bool hasActionPoint()               { return state().player()->getRemainingActionCooldownTicks() == 0; }

        const double       m_helicopterIteration;  // size of movement emulation increment
        Point              m_ifvCoverPos;

        bool doAttack(Callback shouldAbort, Callback shouldProceed, const VehicleGroup& attackTarget);

        bool shiftAircraftAway();
		bool prepareCoverByAircraft();

		bool isPathToIfvFree();
        
        Point getActualIfvCoverPos();
		Point getAircraftBypassPoint(const VehicleGroup& fighters, const VehicleGroup& helicopters, Point defendDestination);
        Point getFightersTargetPoint(const VehicleGroup& attackTarget, const VehicleGroup& attackWith);

		virtual bool isCompatibleWith(const Goal* interrupted) override;


    public:
        DefendHelicoptersFromRush(State& state, GoalManager& goalManager);

	};
}

