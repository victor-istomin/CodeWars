#include <algorithm>

#define PI 3.14159265358979323846
#define _USE_MATH_DEFINES
#include <cmath>

#include "goalDefendHelicopter.h"
#include "state.h"

#include "model/ActionType.h"
#include "model/VehicleType.h"
#include "model/Move.h"
#include "model/Game.h"

using namespace goals;
using namespace model;

namespace
{
    const int MAX_DEFEND_TICK = 100;
}

const VehicleGroup& goals::DefendHelicopters::ifvGroup(State& state)
{
    return state.teammates(model::VEHICLE_IFV);
}

const VehicleGroup& goals::DefendHelicopters::helicopterGroup(State& state)
{
    return state.teammates(model::VEHICLE_HELICOPTER);
}

const VehicleGroup& goals::DefendHelicopters::fighterGroup(State& state)
{
	return state.teammates(model::VEHICLE_FIGHTER);
}

goals::DefendHelicopters::DefendHelicopters(State& state)
{
    auto abortCheck = [](State& state) 
    { 
        assert(!state.isMoveCommitted());   // conflict in move logic!
        return state.world()->getTickIndex() > MAX_DEFEND_TICK && !state.isMoveCommitted();
    };

    auto hasActionPoint = [](State& state) { return state.player()->getRemainingActionCooldownTicks() == 0; };


	auto isPathToIfvFree = [](State& state)
	{
		const VehicleGroup& fighters  = fighterGroup(state);
		const Point helicoptersCenter = helicopterGroup(state).m_center;
		const Point ifvCenter         = ifvGroup(state).m_center;
		const Point fighterCenter     = fighters.m_center;

		double angleBetween = std::abs(Vec2d::angleBetween(
			Vec2d::fromPoint(fighterCenter - helicoptersCenter),
			Vec2d::fromPoint(ifvCenter - helicoptersCenter)));

		if (helicoptersCenter.getDistanceTo(fighterCenter) > helicoptersCenter.getDistanceTo(ifvCenter)
			|| angleBetween >= (PI / 3 - Point::k_epsilon))
		{
			return true;   // mid-air collision is unlikely
		}

		return false;
	};

	auto shiftAircraft = [abortCheck, hasActionPoint, isPathToIfvFree, this](State& state)
	{
		if (isPathToIfvFree(state))
			return true;  // nothing to move

		const VehicleGroup& fighters  = fighterGroup(state);
		const Point helicoptersCenter = helicopterGroup(state).m_center;
		const Point ifvCenter         = ifvGroup(state).m_center;
		const Point fighterCenter     = fighters.m_center;

		// select and move fighters in order to avoid mid-air collision

		state.setSelectAction(fighters.m_rect, VEHICLE_FIGHTER);

		const double near = 1.1;
		const double far = 1.1;

		Point solutions[] = { fighterCenter + (Point(fighters.m_rect.width(), 0)  * near),       // right
							  fighterCenter + (Point(0, fighters.m_rect.height()) * near),       // down
							  fighterCenter - (Point(fighters.m_rect.width(), 0)  * near),       // left
							  fighterCenter - (Point(0, fighters.m_rect.height()) * near),       // up

							  fighterCenter + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * near),        // right down
							  fighterCenter - (Point(fighters.m_rect.width(), fighters.m_rect.height()) * near),        // left up

							  fighterCenter - (Point(fighters.m_rect.width(), 0)  * far),        // far left
							  fighterCenter - (Point(0, fighters.m_rect.height()) * far),        // far up
							  fighterCenter + (Point(fighters.m_rect.width(), 0)  * far),        // far right
							  fighterCenter + (Point(0, fighters.m_rect.height()) * far),        // far down

							  fighterCenter + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far),         // far right down
							  fighterCenter - (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far),         // far left up

							  fighterCenter + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far * far) }; // very far down and right

		auto solutionIt = std::find_if(std::begin(solutions), std::end(solutions), 
			[&helicoptersCenter, &ifvCenter, &fighterCenter, &fighters](const Point& solution) 
		{
			double fightersSize = std::max(fighters.m_rect.width(), fighters.m_rect.height());

			if (   solution.getDistanceTo(ifvCenter)         < fightersSize    // don't move over IFV
				|| solution.getDistanceTo(helicoptersCenter) < fightersSize )  // ... or helicopters
				return false;

			Vec2d fighter2helics   = Vec2d::fromPoint(helicoptersCenter - fighterCenter);
			Vec2d fighter2solution = Vec2d::fromPoint(solution          - fighterCenter);

			Vec2d helics2fighter   = Vec2d::fromPoint(fighterCenter - helicoptersCenter);
			Vec2d helics2solution  = Vec2d::fromPoint(solution      - helicoptersCenter);

			return std::abs(Vec2d::angleBetween(fighter2helics, fighter2solution)) >= (PI / 3 - Point::k_epsilon)
				&& ( std::abs(Vec2d::angleBetween(helics2fighter, helics2solution)) >= (PI / 3 - Point::k_epsilon)
					|| helicoptersCenter.getDistanceTo(solution) > helicoptersCenter.getDistanceTo(ifvCenter));
		});

		const Point solution    = solutionIt != std::end(solutions) ? *solutionIt : *std::rbegin(solutions);
		const Vec2d solutonPath = Vec2d::fromPoint(solution - fighterCenter);

		this->pushNextStep(abortCheck, hasActionPoint, [solutonPath](State& state)
		{
			state.setMoveAction(solutonPath);
			return true;
		}, "move fighters");

		return true;
	};

    auto selectHelicopters = [](State& state) 
    { 
        state.setSelectAction(helicopterGroup(state).m_rect, VEHICLE_HELICOPTER);
        return true;
    };

    auto moveToJoinPoint = [](State& state)
    {
        const Point joinPoint  = ifvGroup(state).m_center;
        const Point selfCenter = helicopterGroup(state).m_center;

        double distanceTo = selfCenter.getDistanceTo(joinPoint);
        double eta        = distanceTo / state.game()->getHelicopterSpeed();   // TODO : use correct prediction

        state.setMoveAction(Vec2d::fromPoint(joinPoint - selfCenter));
        return true;
    };

	auto canMove = [hasActionPoint, isPathToIfvFree](State& s) { return hasActionPoint(s) && isPathToIfvFree(s); };

    pushBackStep(abortCheck, hasActionPoint, shiftAircraft,     "ensure no aircraft collision");
    pushBackStep(abortCheck, canMove,        selectHelicopters, "select helicopters");
    pushBackStep(abortCheck, hasActionPoint, moveToJoinPoint,   "move helicopters to IFV");
}



