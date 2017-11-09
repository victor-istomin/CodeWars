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
		const VehicleGroup& fighters    = fighterGroup(state);
        const VehicleGroup& helicopters = helicopterGroup(state);

		const Point helicoptersCenter = helicopters.m_center;
		const Point ifvCenter         = ifvGroup(state).m_center;
		const Point fighterCenter     = fighters.m_center;

		double angleBetween = std::abs(Vec2d::angleBetween(
			Vec2d::fromPoint(fighterCenter - helicoptersCenter),
			Vec2d::fromPoint(ifvCenter - helicoptersCenter)));

		if (helicoptersCenter.getDistanceTo(fighterCenter) > helicoptersCenter.getDistanceTo(ifvCenter) || angleBetween > (PI / 2))
		{
			return true;   // mid-air collision is unlikely
		}

        const Rect& helicoptersRect = helicopters.m_rect;
        const Rect& fightersRect = fighters.m_rect;

        double iterationSize = std::min(state.constants().m_helicoprerRadius, state.game()->getHelicopterSpeed()) / 2;
        return canMoveRectTo(helicoptersCenter, ifvCenter, helicoptersRect, fightersRect, iterationSize);
	};

	auto shiftAircraft = [abortCheck, hasActionPoint, isPathToIfvFree, this](State& state)
	{
		if (isPathToIfvFree(state))
			return true;  // nothing to move

		const VehicleGroup& fighters    = fighterGroup(state);
        const VehicleGroup& helicopters = helicopterGroup(state);

		const Point helicoptersCenter = helicopters.m_center;
		const Point ifvCenter         = ifvGroup(state).m_center;
		const Point fighterCenter     = fighters.m_center;

		// select and move fighters in order to avoid mid-air collision

		state.setSelectAction(fighters.m_rect, VEHICLE_FIGHTER);

		const double near = 1.2;
		const double far  = 2.4;

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
			[&helicoptersCenter, &ifvCenter, &fighterCenter, &fighters, &helicopters, &state] (const Point& solution) 
		{
			double fightersSize = std::max(fighters.m_rect.width(), fighters.m_rect.height());

            Vec2d solutionPath = Vec2d::fromPoint(solution - fighterCenter);
            Rect  proposedRect = fighters.m_rect + solutionPath;

			if (   proposedRect.m_topLeft.m_x < 0
                || proposedRect.m_topLeft.m_y < 0
                || solution.getDistanceTo(ifvCenter)         < fightersSize    // don't move over IFV
				|| solution.getDistanceTo(helicoptersCenter) < fightersSize )  // ... or helicopters
            { 
				return false;
            }

			Vec2d fighter2helics   = Vec2d::fromPoint(helicoptersCenter - fighterCenter);
			Vec2d fighter2solution = Vec2d::fromPoint(solution          - fighterCenter);

			Vec2d helics2fighter   = Vec2d::fromPoint(fighterCenter - helicoptersCenter);
			Vec2d helics2solution  = Vec2d::fromPoint(solution      - helicoptersCenter);

            double iterationSize = std::min(state.constants().m_helicoprerRadius, state.game()->getHelicopterSpeed()) / 2;


            return canMoveRectTo(helicoptersCenter, ifvCenter, helicopters.m_rect, fighters.m_rect + solutionPath, iterationSize)
                && canMoveRectTo(fighterCenter,     solution,  fighters.m_rect,    helicopters.m_rect,             iterationSize);
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

bool DefendHelicopters::canMoveRectTo(const Point& from, const Point& to, const Rect& fromRect, const Rect& obstacleRect, double iterationSize)
{
    // TODO - it's possible to perform more careful check
    Vec2d direction = Vec2d::fromPoint(to - from).truncate(iterationSize);
    int stepsTotal = static_cast<int>(std::ceil(from.getDistanceTo(to) / iterationSize));

    bool isPathFree = true;

    for (int i = 0; i < stepsTotal && isPathFree; ++i)
    {
        Rect destination = fromRect + direction * i * iterationSize;
        isPathFree = !destination.overlaps(obstacleRect);
    }

    return isPathFree;
}


