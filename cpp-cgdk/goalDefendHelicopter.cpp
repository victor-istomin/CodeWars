#include <algorithm>

#define PI 3.14159265358979323846
#define _USE_MATH_DEFINES
#include <cmath>
#include <typeinfo>

#include "goalDefendHelicopter.h"
#include "GoalMixTanksAndHealers.h"
#include "goalUtils.h"
#include "goalManager.h"

#include "state.h"

#include "model/ActionType.h"
#include "model/VehicleType.h"
#include "model/Move.h"
#include "model/Game.h"

using namespace goals;
using namespace model;


bool DefendHelicopters::doAttack(Callback shouldAbort, Callback shouldProceed, const VehicleGroup& attackWith, const VehicleGroup& attackTarget)
{
    if (attackWith.m_units.empty() || attackTarget.m_units.empty())
        return true;   // do nothing if not possible to attach (don't block entire goal)

    const Rect& attackRect = attackTarget.m_rect;

    static const double sqrt2 = std::sqrt(2);
    VehiclePtr firstUnit = attackWith.m_units.front().lock();

    double aerialAttackRange = firstUnit->getAerialAttackRange();
    double radius = firstUnit->getRadius();

    const Point& displacement1 = Point(  aerialAttackRange - radius,  aerialAttackRange - radius) / sqrt2 / 1.5;
    const Point& displacement2 = Point(-(aerialAttackRange - radius), aerialAttackRange - radius) / sqrt2 / 1.5;
    const Point& displacement3 = Point(  aerialAttackRange - radius,  -(aerialAttackRange - radius)) / sqrt2 / 1.5;
    const Point& displacement4 = Point(-(aerialAttackRange - radius), -(aerialAttackRange - radius)) / sqrt2 / 1.5;

    Point attackPoints[] = 
    { 
        attackTarget.m_center, 

        attackRect.m_topLeft, attackRect.m_bottomRight, attackRect.bottomLeft(), attackRect.topRight(),
        attackRect.m_topLeft + displacement1, attackRect.m_bottomRight + displacement1, attackTarget.m_center + displacement1,
        attackRect.bottomLeft() + displacement1, attackRect.topRight() + displacement1,
    };

    std::sort(std::begin(attackPoints), std::end(attackPoints), 
        [&attackWith](const Point& left, const Point& right) { return attackWith.m_center.getSquareDistance(left) < attackWith.m_center.getSquareDistance(right); });

    const VehicleGroup& obstacle = helicopterGroup();

    std::stable_partition(std::begin(attackPoints), std::end(attackPoints),
        [this, &attackWith, &obstacle](const Point& p) { return attackWith.isPathFree(p, Obstacle(obstacle), m_helicopterIteration); });

    const Point& targetPoint = attackPoints[0];
    Vec2d path = targetPoint - attackWith.m_center;

	state().setSelectAction(attackWith);
	pushNextStep(shouldAbort, [this] {return hasActionPoint(); }, [this, path]() { state().setMoveAction(path); return true; }, "make attack move");

    static const int MIN_TICKS_GAP = 10;
    int nTicksGap = std::max(MIN_TICKS_GAP, static_cast<int>(path.length() / firstUnit->getMaxSpeed() / 4));

    pushBackStep(shouldAbort, WaitSomeTicks{ nTicksGap }, DoNothing(), "wait next attack", StepType::ALLOW_MULTITASK);
    pushBackStep(shouldAbort, shouldProceed, std::bind(&DefendHelicopters::doAttack, this, shouldAbort, shouldProceed, std::cref(attackWith), std::cref(attackTarget)), "attack again");
    return true;
}

bool DefendHelicopters::abortCheck()
{
    bool isFightersBeaten = state().alliens(VehicleType::FIGHTER).m_healthSum < (state().teammates(VehicleType::HELICOPTER).m_healthSum * MIN_HEALTH_FACTOR);
    return state().world()->getTickIndex() > MAX_DEFEND_TICK || isFightersBeaten;
}

DefendHelicopters::DefendHelicopters(State& state, const GoalManager& goalManager)
    : Goal(state)
    , m_helicopterIteration(std::min(state.constants().m_helicoprerRadius, state.game()->getHelicopterSpeed()) / 2)
    , m_goalManager(goalManager)
{
    auto abortCheckFn     = [this]() { return abortCheck(); };
    auto hasActionPointFn = [this]() { return this->state().hasActionPoint(); };

    auto moveToJoinPoint = [this, hasActionPointFn, abortCheckFn]()
    {
        const Point joinPoint  = getActualIfvCoverPos();
        const Point selfCenter = helicopterGroup().m_center;

		this->state().setSelectAction(helicopterGroup());

		Vec2d movement = joinPoint - selfCenter;
		pushNextStep(abortCheckFn, hasActionPointFn, [this, movement]() { this->state().setMoveAction(movement); return true; }, "move helicopters to IFV");

        return true;
    };

    pushBackStep(abortCheckFn, hasActionPointFn, [this](){ return shiftAircraftAway();}, "ensure no aircraft collision");

    auto canMoveHelicopters = [this]() { return hasActionPoint() && isPathToIfvFree(); };
	pushBackStep(abortCheckFn, canMoveHelicopters, moveToJoinPoint,   "move helicopters to IFV", StepType::ALLOW_MULTITASK);

	pushBackStep(abortCheckFn, WaitUntilStops(helicopterGroup()), DoNothing(), "finish helicopters move", StepType::ALLOW_MULTITASK);

	pushBackStep(abortCheckFn, hasActionPointFn, [this]() { return prepareCoverByAircraft(); }, "fighter: prepare defend pos");

	const auto& fighters = fighterGroup();

	auto isReadyForAttack = [this, hasActionPointFn](const VehicleGroup& attackWith, const VehicleGroup& attackTarget)
	{ 
		if (attackWith.m_units.empty() || attackTarget.m_units.empty())
			return true;   // do nothing if not possible to attach (don't block entire goal)

        VehiclePtr firstUnit = attackWith.m_units.front().lock();

        int minCooldownTicks = firstUnit->getRemainingAttackCooldownTicks();
        for (const VehicleCache& nextUnit : attackWith.m_units)
            minCooldownTicks = std::min(minCooldownTicks, nextUnit.lock()->getRemainingAttackCooldownTicks());

        static const int PREPARE_TICKS = 10;
        bool isAboutToBeReady = minCooldownTicks < PREPARE_TICKS;

        return hasActionPointFn() && isAboutToBeReady;
	};

    auto isNear = [](const VehicleGroup& attackWith, const VehicleGroup& attackTarget, double distanceLimit)
    {
        return attackWith.m_center.getDistanceTo(attackTarget.m_rect.m_topLeft) < distanceLimit;
    };

	auto doAttackFighters = [this, abortCheckFn, isReadyForAttack]() 
    { 
        const VehicleGroup& attacker = fighterGroup();
        const VehicleGroup& target   = allienFighters();

        return doAttack(abortCheckFn, std::bind(isReadyForAttack, std::cref(attacker), std::cref(target)), attacker, target); 
    };
    
    auto shouldStartAttack = [&isNear, &isReadyForAttack, this]()
    {
        return isNear(fighterGroup(), allienFighters(), 5 * fighterGroup().m_rect.width());
    };

	pushBackStep(abortCheckFn, shouldStartAttack, doAttackFighters, "fighter: start attacking enemy fighters");
}

bool DefendHelicopters::isCompatibleWith(const Goal* interrupted)
{
	return typeid(*interrupted) == typeid(MixTanksAndHealers);
}

Point DefendHelicopters::getActualIfvCoverPos()
{
    if (m_ifvCoverPos == Point())
    {
        const GoalManager::Goals& currentGoals = m_goalManager.currentGoals();

        auto isMixGoal = [](const GoalManager::GoalHolder& goal) { return typeid(*goal.m_goal) == typeid(MixTanksAndHealers); };
        auto itMixGoal = std::find_if(currentGoals.begin(), currentGoals.end(), isMixGoal);

        if (itMixGoal != currentGoals.end())
        {
            const MixTanksAndHealers* mixGoal = static_cast<const MixTanksAndHealers*>(itMixGoal->m_goal.get());
            m_ifvCoverPos = mixGoal->getFinalDestination(VehicleType::IFV);
        }
    }

    return m_ifvCoverPos != Point() ? m_ifvCoverPos : ifvGroup().m_center;
}


bool DefendHelicopters::isPathToIfvFree()
{
	return helicopterGroup().isPathFree(getActualIfvCoverPos(), Obstacle(fighterGroup()), m_helicopterIteration);
}

bool DefendHelicopters::shiftAircraftAway()
{
	if (isPathToIfvFree())
		return true;  // nothing to move

	const VehicleGroup& fighters    = fighterGroup();
	const VehicleGroup& helicopters = helicopterGroup();

	const Point helicoptersCenter = helicopters.m_center;
	const Point ifvCenter         = getActualIfvCoverPos();
	const Point fighterCenter     = fighters.m_center;

	// select and move fighters in order to avoid mid-air collision

	state().setSelectAction(fighters);

	const double near = 1.2;
	const double far = 2.4;

	Point solutions[] = 
	{ 
		tankGroup().m_center,                            // it's fine idea to defend tanks

		fighterCenter + (Point(fighters.m_rect.width(), 0)  * near),       // right
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

		fighterCenter + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far * far)    // very far down and right
	}; 

	auto solutionIt = std::find_if(std::begin(solutions), std::end(solutions),
		[&helicoptersCenter, &ifvCenter, &fighterCenter, &fighters, &helicopters, this](const Point& solution)
	{
		double fightersSize = std::max(fighters.m_rect.width(), fighters.m_rect.height());

		Vec2d solutionPath = Vec2d::fromPoint(solution - fighterCenter);
		Rect  proposedRect = fighters.m_rect + solutionPath;

		if (!this->state().isCorrectPosition(proposedRect)
			|| solution.getDistanceTo(ifvCenter) < fightersSize    // don't move over IFV
			|| solution.getDistanceTo(helicoptersCenter) < fightersSize)  // ... or helicopters
		{
			return false;
		}

		Vec2d fighter2helics   = Vec2d::fromPoint(helicoptersCenter - fighterCenter);
		Vec2d fighter2solution = Vec2d::fromPoint(solution - fighterCenter);

		Vec2d helics2fighter  = Vec2d::fromPoint(fighterCenter - helicoptersCenter);
		Vec2d helics2solution = Vec2d::fromPoint(solution - helicoptersCenter);

		VehicleGroupGhost fightersGhost = VehicleGroupGhost(fighters, fighter2solution);  // TODO

		return helicopters.isPathFree(ifvCenter, Obstacle(fightersGhost), m_helicopterIteration)
			&& fighters.isPathFree(solution, Obstacle(helicopters), m_helicopterIteration);
	});

	const Point solution = solutionIt != std::end(solutions) ? *solutionIt : *std::rbegin(solutions);
	const Vec2d solutionPath = Vec2d::fromPoint(solution - fighterCenter);

	pushNextStep([this]() { return abortCheck(); }, 
	             [this]() { return state().hasActionPoint(); }, 
	             [solutionPath, this]() { this->state().setMoveAction(solutionPath); return true; }, "move fighters");

	return true;

}

bool DefendHelicopters::prepareCoverByAircraft()
{
	const VehicleGroup& fighters = fighterGroup();
	const VehicleGroup& helicopters = helicopterGroup();

	Vec2d pathFromHelicopters = Vec2d(allienFighters().m_center - helicopterGroup().m_center).normalize() * fighterGroup().m_rect.height() * 1.8;
	Point defendDestination = helicopters.m_center + pathFromHelicopters.toPoint<Point>();

	bool isMovePossible = false;

	auto abortCheckFn     = [this]() { return abortCheck(); };
	auto hasActionPointFn = [this]() { return state().hasActionPoint(); };

	if (fighters.isPathFree(defendDestination, Obstacle(helicopters), m_helicopterIteration))
	{
		isMovePossible = true;

		Vec2d movement = defendDestination - fighters.m_center;
		pushNextStep(abortCheckFn, hasActionPointFn,
			[this, movement]() { this->state().setMoveAction(movement); return true; }, "fighter: defend helicopters straight move");
	}
	else
	{
		// should find a path around helicopters
		Point bypassPoint = getAircraftBypassPoint(fighters, helicopters, defendDestination);

		if (bypassPoint != Point())
		{
			isMovePossible = true;

			Vec2d finalPath = defendDestination - bypassPoint;
			auto isPathBecameFree = [this, hasActionPointFn, defendDestination, bypassPoint]()
			{
				return hasActionPointFn()
					&& fighterGroup().m_center.getDistanceTo(bypassPoint) < 1
					&& fighterGroup().isPathFree(defendDestination, Obstacle(helicopterGroup()), m_helicopterIteration);
			};

			// push 2 steps in LIFO order: first stage move and then finalMove

			// TODO: allow multitasking on wait here?
			pushNextStep(abortCheckFn, isPathBecameFree,
				[this, finalPath]() { this->state().setMoveAction(finalPath); return true; }, "fighter: defend helicopters 2nd stage");

			Vec2d firstStagePath = bypassPoint - fighters.m_center;
			pushNextStep(abortCheckFn, hasActionPointFn,
				[this, firstStagePath]() { this->state().setMoveAction(firstStagePath); return true; }, "fighter: defend helicopters 1st stage");
		}
	}

	if (isMovePossible)
	{
		this->state().setSelectAction(fighters);
	}

	return true;
}


Point DefendHelicopters::getAircraftBypassPoint(const VehicleGroup& fighters, const VehicleGroup& helicopters, Point defendDestination)
{
	const double near = 1.4;
	const double far = 2.8;

	Point solutions[] =
	{
		fighters.m_center + (Point(fighters.m_rect.width(), 0) * near),                           // right
		fighters.m_center + (Point(0, fighters.m_rect.height()) * near),                          // down
		fighters.m_center + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * near),    // down right
		fighters.m_center + (Point(-fighters.m_rect.width(), fighters.m_rect.height()) * near),   // down left

		fighters.m_center + (Point(fighters.m_rect.width(), 0) * far),                            // far right
		fighters.m_center + (Point(0, fighters.m_rect.height()) * far),                           // far down
		fighters.m_center + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far),     // far down right
		fighters.m_center + (Point(-fighters.m_rect.width(), fighters.m_rect.height()) * far),    // far down left

		fighters.m_center + (Point(fighters.m_rect.width(), 0) * far * far),                      // far far right
		fighters.m_center + (Point(0, fighters.m_rect.height()) * far * far),                     // far far down
	};

	auto solutionIt = std::find_if(std::begin(solutions), std::end(solutions),
		[&helicopters, &fighters, &defendDestination, this](const Point& tmpPos)
	{
		Vec2d dFighters = tmpPos - fighters.m_center;
		VehicleGroupGhost fightersGhost = VehicleGroupGhost(fighters, dFighters);

		return tmpPos.m_x > 0 && tmpPos.m_y > 0
			&& fighters.isPathFree(tmpPos, Obstacle(helicopters), m_helicopterIteration)
			&& fightersGhost.isPathFree(defendDestination, Obstacle(helicopters), m_helicopterIteration);
	});

	return solutionIt != std::end(solutions) ? *solutionIt : Point();
}

