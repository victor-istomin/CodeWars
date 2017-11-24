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

        const double       m_helicopterIteration;  // size of movement emulation increment
        const GoalManager& m_goalManager;
        Point              m_ifvCoverPos;

        bool doAttack(Callback shouldAbort, Callback shouldProceed, const VehicleGroup& attackWith, const VehicleGroup& attackTarget);
		bool shiftAircraftAway();
		bool prepareCoverByAircraft();

		bool isPathToIfvFree();
        
        Point getActualIfvCoverPos();
		Point getAircraftBypassPoint(const VehicleGroup &fighters, const VehicleGroup& helicopters, Point defendDestination);

		virtual bool isCompatibleWith(const Goal* interrupted) override;


    public:
        DefendHelicopters(State& state, const GoalManager& goalManager);

	};
}

