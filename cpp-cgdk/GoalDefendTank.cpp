#include "GoalDefendTank.h"
#include "model/Vehicle.h"
#include "model/Game.h"

#include "state.h"
#include "VehicleGroup.h"
#include "GoalMixTanksAndHealers.h"
#include "goalDefendHelicoptersFromRush.h"
#include "goalManager.h"
#include "goalUtils.h"

#include <vector>
#include <algorithm>
#include <type_traits>
#include <functional>

using namespace model;
using namespace goals;

bool GoalDefendTank::abortCheck() const
{
    int tickIndex = state().world()->getTickIndex();
    return tickIndex > MAX_DEFEND_TICK 
        || (isHelicoptersBeaten() && (tickIndex - m_lastAttackTick > MAX_HEAL_TICKS));
}

bool GoalDefendTank::isHelicoptersBeaten() const
{
    return allienHelicopters().m_healthSum < (tankGroup().m_healthSum * MIN_HEALTH_FACTOR);
}

bool GoalDefendTank::resolveFightersHelicoptersConflict()
{
    const VehicleGroup& fighters    = fighterGroup();
    const VehicleGroup& helicopters = helicopterGroup();

	Rect safeFightersRect    = fighters.m_rect.inflate(state().game()->getVehicleRadius());
	Rect safeHelicoptersRect = helicopters.m_rect.inflate(state().game()->getVehicleRadius());

    if (safeFightersRect.overlaps(safeHelicoptersRect))
    {
        Vec2d fightersDirection    = Vec2d(fighters.m_center - helicopters.m_center).truncate(state().game()->getFighterSpeed());
        Vec2d helicoptersDirection = Vec2d(helicopters.m_center - fighters.m_center).truncate(state().game()->getHelicopterSpeed());

        static const double FIGHTER_DISPLACEMENT    = 300;
        static const double HELICOPTER_DISPLACEMENT = 150;

        const int maxIterations = static_cast<int>(std::max(std::ceil(FIGHTER_DISPLACEMENT / fightersDirection.length()), 
                                                            std::ceil(HELICOPTER_DISPLACEMENT / helicoptersDirection.length())));

        Vec2d fighterSolution;
        Vec2d helicopterSolution;

        for (int i = 1; i < maxIterations; ++i)
        {
            Vec2d proposedFighterSolution = fightersDirection * i;
            Rect  proposedFighterRect     = safeFightersRect + proposedFighterSolution;

            if (state().isCorrectPosition(proposedFighterRect) && !proposedFighterRect.overlaps(safeHelicoptersRect))
            {
                fighterSolution = proposedFighterSolution;
                break;
            }

            Vec2d proposedHelicopterSolution = helicoptersDirection * i;
            Rect  proposedHelicoptersRect = safeHelicoptersRect + proposedHelicopterSolution;
            if (state().isCorrectPosition(proposedHelicoptersRect) && !safeFightersRect.overlaps(proposedHelicoptersRect))
            {
                helicopterSolution = proposedHelicopterSolution;
                break;
            }
        }

        m_lastConflictTick = state().world()->getTickIndex();  // TODO: HACK - remove this?
        auto waitUntilNoCollision = [&fighters, &helicopters, this]()
        {
            int ticksElapsed = state().world()->getTickIndex() - m_lastConflictTick;

            return fighters.m_units.empty()
                || helicopters.m_units.empty()
                || ticksElapsed > MAX_RESOLVE_CONFLICT_TICKS
                || !fighters.m_rect.overlaps(helicopters.m_rect);
        };

        // TODO: apply both for better confidence?

        if (fighterSolution.length() > Point::k_epsilon)
        {
            state().setSelectAction(fighters);

            // next steps will be actually pushed in LIFO order! So, start move and then wait

            pushNextStep([this]() { return abortCheck(); }, waitUntilNoCollision, DoNothing(), "dt: wait for fighter collision resolve", StepType::ALLOW_MULTITASK);

            pushNextStep([this]() { return abortCheck(); },
                         [this]() { return state().hasActionPoint(); },
                         [this, fighterSolution]() { state().setMoveAction(fighterSolution); return true; },
                         "dt: resolve collision - move fighters away");
        }
        else if (helicopterSolution.length() > Point::k_epsilon)
        {
            state().setSelectAction(helicopters);

            // next steps will be actually pushed in LIFO order! So, start move and then wait

            pushNextStep([this]() { return abortCheck(); }, waitUntilNoCollision, DoNothing(), "dt: wait for helicopter collision resolve", StepType::ALLOW_MULTITASK);

            pushNextStep([this]() { return abortCheck(); },
                         [this]() { return state().hasActionPoint(); },
                         [this, helicopterSolution]() { state().setMoveAction(helicopterSolution); return true; },
                         "dt: resolve collision - move helicopters away");
        }
    }    

    return true;
}


bool GoalDefendTank::shiftAircraft()
{
    const VehicleGroup& enemyHelicopters = allienHelicopters();
    const VehicleGroup& tanks = tankGroup();
    const VehicleGroup& fighters = fighterGroup();
    const VehicleGroup& helicopters = helicopterGroup();

    if (helicopters.isPathFree(tankGroup().m_center, Obstacle(fighterGroup()), m_helicopterIteration))
        return true;   // no need to shift

    static const double near = 1.2;
    static const double far  = 2.4;

    const Point solutions[] = 
    {
        enemyHelicopters.m_center, enemyHelicopters.m_rect.m_topLeft, enemyHelicopters.m_rect.topRight(),
        (enemyHelicopters.m_center + tanks.m_center) / 2, (enemyHelicopters.m_center + fighters.m_center + tanks.m_center) / 3,

        fighters.m_center + Point(fighters.m_rect.width(), 0) * near,     // right
        fighters.m_center + Point(fighters.m_rect.width(), 0) * far,      // far right
        fighters.m_center + Point(-fighters.m_rect.width(), 0) * near,    // left
        fighters.m_center + Point(-fighters.m_rect.width(), 0) * far,     // far left
        fighters.m_center + Point(0, fighters.m_rect.height()) * near,     // down
        fighters.m_center + Point(0, fighters.m_rect.height()) * far,      // far down
        fighters.m_center + Point(0, -fighters.m_rect.height()) * near,    // up
        fighters.m_center + Point(0, -fighters.m_rect.height()) * far,     // far up

        fighters.m_center + Point(fighters.m_rect.width(), -fighters.m_rect.height()) * near,    // up right
        fighters.m_center + Point(fighters.m_rect.width(), -fighters.m_rect.height()) * far,     // far up right
        fighters.m_center + Point(-fighters.m_rect.width(), -fighters.m_rect.height()) * near,   // up left
        fighters.m_center + Point(-fighters.m_rect.width(), -fighters.m_rect.height()) * far,    // far up left

        fighters.m_center + Point(fighters.m_rect.width(), fighters.m_rect.height()) * near,    // down right
        fighters.m_center + Point(fighters.m_rect.width(), fighters.m_rect.height()) * far,     // far down right
        fighters.m_center + Point(-fighters.m_rect.width(), fighters.m_rect.height()) * near,   // down left
        fighters.m_center + Point(-fighters.m_rect.width(), fighters.m_rect.height()) * far,    // far down left
    };

    std::vector<Point> correctSolutons;
    correctSolutons.reserve(std::extent<decltype(solutions)>::value);

    std::copy_if(std::begin(solutions), std::end(solutions), std::back_inserter(correctSolutons), 
        [this, &fighters, &helicopters, &tanks](const Point& proposed)
    {
        Vec2d displacement = proposed - fighters.m_center;
        Rect  proposedRect = fighters.m_rect + displacement;

        return state().isCorrectPosition(proposedRect) 
            && fighters.isPathFree(proposed, Obstacle(helicopters), m_helicopterIteration)
            && helicopters.isPathFree(tanks.m_center, Obstacle(VehicleGroupGhost(fighters, displacement)), m_helicopterIteration);
    });

    // sort by distance to tank (less priority) then by distance to enemy helicopters, then by distance to fighters (most priority)
    std::sort(correctSolutons.begin(), correctSolutons.end(), [&tanks](const Point& left, const Point& right) 
    {
        return tanks.m_center.getSquareDistance(left) < tanks.m_center.getSquareDistance(right);
    });

    std::stable_sort(correctSolutons.begin(), correctSolutons.end(), [&enemyHelicopters](const Point& left, const Point& right)
    {
        return enemyHelicopters.m_center.getSquareDistance(left) < enemyHelicopters.m_center.getSquareDistance(right);
    });

    std::stable_sort(correctSolutons.begin(), correctSolutons.end(), [&fighters](const Point& left, const Point& right)
    {
        return fighters.m_center.getSquareDistance(left) < fighters.m_center.getSquareDistance(right);
    });

    if (!correctSolutons.empty())
    {
        state().setSelectAction(fighters);

        Point bestSolution = correctSolutons.front();
        pushNextStep([this]() { return abortCheck(); }, 
                     [this]() { return state().hasActionPoint(); },
                     [this, bestSolution, &fighters]() { state().setMoveAction(bestSolution - fighters.m_center); return true; },
                     "fighters: defend tank move");
    }  
    
    // TODO else resolve conflict

    return true;
}

bool GoalDefendTank::moveHelicopters()
{
    state().setSelectAction(helicopterGroup());
    
    pushNextStep([this]() { return abortCheck(); },
                 [this]() { return state().hasActionPoint(); },
                 [this]() { state().setMoveAction(tankGroup().m_center - helicopterGroup().m_center); return true; },
                 "helicopters: defend tank move");

    return true;
}

bool GoalDefendTank::startFightersAttack()
{
    if (fighterGroup().m_center.getDistanceTo(allienHelicopters().m_center) <= m_maxAgressiveDistance)
    {
        pushNextStep([this]() { return abortCheck(); },
                     [this]() { return state().hasActionPoint(); },
                     [this]() { return loopFithersAttack(); },
                     "fighters: defend tank - attack enemy");
    }

    return true;
}

bool GoalDefendTank::loopFithersAttack()
{
    const VehicleGroup& target      = allienHelicopters();
    const VehicleGroup& fighters    = fighterGroup();
    const VehicleGroup& tanks       = tankGroup();
    const VehicleGroup& helicopters = helicopterGroup();

    if (target.m_units.empty() || fighters.m_units.empty() || tankGroup().m_units.empty())
        return true;

    Point targetPoint = target.m_center;

    Rect fightersPosRect = fighters.m_rect + (targetPoint - fighters.m_center);
    
    if (!fighters.m_rect.overlaps(helicopters.m_rect) && (fightersPosRect).overlaps(helicopters.m_rect))
    {
        // not yet collide with teammate helicopters, try avoid further collisions
        const Point fightersSize = Point(fighters.m_rect.width(), fighters.m_rect.height());
        const Point fightersWidth = Point(fighters.m_rect.width(), 0);
        const Point fightersHeight = Point(0, fighters.m_rect.height());

        const Point attackPoints[] =
        {
            target.m_center,   // most priority

            target.m_center - fightersSize / 4,                        // closer to top left
            target.m_center - fightersSize / 4 + fightersWidth / 2,    // closer to top right
            target.m_center + fightersSize / 4 - fightersWidth / 2,    //   ... to bottom left
            target.m_center + fightersSize / 4,                        //   ... to bottom right

            target.m_rect.m_topLeft + fightersSize / 3,
            target.m_rect.m_bottomRight - fightersSize / 3,
            target.m_rect.topRight() + fightersSize / 3,
            target.m_rect.bottomLeft() - fightersSize / 3,
        };

        auto solutionIt = std::find_if(std::begin(attackPoints), std::end(attackPoints), 
            [&fighters, &helicopters, this](const Point& p)
        {
            Rect proposedRect = fighters.m_rect + (p - fighters.m_center);
            return !helicopters.m_rect.overlaps(proposedRect) 
                && fighters.isPathFree(p, Obstacle(helicopters), m_helicopterIteration);
        });

        if (solutionIt != std::end(attackPoints) && !(targetPoint == *solutionIt))
            targetPoint = *solutionIt;
    }
       
    Vec2d movement = targetPoint - fighters.m_center;

    // next 4 steps are pushed in LIFO order, so: 1) select; 2) attack; 3) wait; 4) loop again

    const int WAIT_AMOINT = 10;

    pushNextStep([this]() { return abortCheck(); },
                 [this]() { return state().hasActionPoint(); },
                 [this]() { return loopFithersAttack(); },
                 "fighters: defend tank - loop attack enemy");

    pushNextStep([this]() { return abortCheck(); }, WaitSomeTicks{ state(), WAIT_AMOINT }, []() { return true; }, 
                 "fighters: defend tank - wait for next iteration", StepType::ALLOW_MULTITASK);

    pushNextStep([this]() { return abortCheck(); },
                 [this]() { return state().hasActionPoint(); },
                 [this, movement]() { state().setMoveAction(movement); return true; },
                 "fighters: defend tank - attack move");

    pushNextStep([this]() { return abortCheck(); },
                 [this]() { return state().hasActionPoint(); },
                 [this, &fighters]() { state().setSelectAction(fighters); return true; },
                 "fighters: defend tank - select fighters");

    m_lastAttackTick = state().world()->getTickIndex();
    return true;
}


GoalDefendTank::GoalDefendTank(State& strategyState, GoalManager& goalManager)
    : Goal(strategyState, goalManager)
    , m_helicopterIteration(std::min(strategyState.constants().m_helicoprerRadius, strategyState.game()->getHelicopterSpeed()) / 2)
    , m_maxAgressiveDistance(strategyState.world()->getWidth() / 4)   // slightly less than half of path from center to me
{
    Callback abortCheckFn       = [this]() { return abortCheck(); };
    Callback hasActionPointFn   = [this]() { return state().hasActionPoint(); };
    Callback canMoveHelicopters = [this]() 
    { 
        int conflictTicksLeft = m_lastConflictTick == 0 ? -1 : std::max(0, state().world()->getTickIndex() - m_lastConflictTick - MAX_RESOLVE_CONFLICT_TICKS);

        bool isPathFree = helicopterGroup().isPathFree(tankGroup().m_center, Obstacle(fighterGroup()), m_helicopterIteration);

        return state().hasActionPoint() && (isPathFree || conflictTicksLeft == 0);
    };

    pushBackStep(abortCheckFn, hasActionPointFn,   [this]() { return resolveFightersHelicoptersConflict(); }, "defend tank: ensure no conflicts");
    pushBackStep(abortCheckFn, hasActionPointFn,   [this]() { return shiftAircraft(); }, "defend tank: shift aircraft");
    pushBackStep(abortCheckFn, canMoveHelicopters, [this]() { return moveHelicopters(); }, "defend tank: move helicopters", StepType::ALLOW_MULTITASK);

    auto shouldStart = [this, hasActionPointFn]() 
    { 
        return hasActionPointFn && fighterGroup().m_center.getDistanceTo(allienHelicopters().m_center) <= m_maxAgressiveDistance; 
    };

    pushBackStep(abortCheckFn, shouldStart,   [this]() { return startFightersAttack(); }, "defend tank: attack helicopters", StepType::ALLOW_MULTITASK);

    // TODO: move outside this goal
    pushBackStep(abortCheckFn, hasActionPointFn, [this]() { return resolveFightersHelicoptersConflict(); }, "defend tank: resolve conflicts");

}

GoalDefendTank::~GoalDefendTank()
{
}

bool GoalDefendTank::isCompatibleWith(const Goal* interrupted)
{
    auto isDefendHelicopters = [](const GoalManager::GoalHolder& goal) { return typeid(*goal.m_goal) == typeid(DefendHelicoptersFromRush); };

    const auto& currentGoals = goalManager().currentGoals();
    bool isDefendHelicopterFinished = std::find_if(currentGoals.begin(), currentGoals.end(), isDefendHelicopters) == currentGoals.end();

    return typeid(*interrupted) == typeid(MixTanksAndHealers) && isDefendHelicopterFinished;
}

