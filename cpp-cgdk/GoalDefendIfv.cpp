#include "GoalDefendIfv.h"
#include "model/VehicleType.h"

using namespace model;
using namespace goals;

bool GoalDefendIfv::abortCheck() const
{
    int tickIndex = state().world()->getTickIndex();
    return tickIndex > MAX_DEFEND_TICK || isTanksBeaten();
}

bool GoalDefendIfv::isTanksBeaten() const
{
    return allienTanks().m_healthSum < ifvGroup().m_healthSum * MIN_HEALTH_FACTOR;
}

bool GoalDefendIfv::shiftAircraft()
{
    const VehicleGroup& enemyTanks  = allienTanks();
    const VehicleGroup& ifv         = ifvGroup();
    const VehicleGroup& fighters    = fighterGroup();
    const VehicleGroup& helicopters = helicopterGroup();

    if (helicopters.isPathFree(ifv.m_center, Obstacle(fighters), m_helicopterIteration))
        return true;   // no need to shift

    static const double near = 1.2;
    static const double far = 2.4;

    const Point solutions[] =
    {
        fighters.m_center + Point(fighters.m_rect.width(), 0) * near,     // right
        fighters.m_center + Point(fighters.m_rect.width(), 0) * far,      // far right
        fighters.m_center + Point(-fighters.m_rect.width(), 0) * near,    // left
        fighters.m_center + Point(-fighters.m_rect.width(), 0) * far,     // far left
        fighters.m_center + Point(0, fighters.m_rect.height()) * near,    // down
        fighters.m_center + Point(0, fighters.m_rect.height()) * far,     // far down
        fighters.m_center + Point(0, -fighters.m_rect.height()) * near,   // up
        fighters.m_center + Point(0, -fighters.m_rect.height()) * far,    // far up

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
        [this, &fighters, &helicopters, &ifv](const Point& proposed)
    {
        Point dxdy = proposed - fighters.m_center;
        Rect  proposedRect = fighters.m_rect + dxdy;

        return state().isCorrectPosition(proposedRect)
            && fighters.isPathFree(proposed, Obstacle(helicopters), m_helicopterIteration)
            && helicopters.isPathFree(ifv.m_center, Obstacle(fighters), m_helicopterIteration);
    });

    std::sort(correctSolutons.begin(), correctSolutons.end(), [&fighters](const Point& left, const Point& right)
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


bool GoalDefendIfv::moveHelicopters()
{
    state().setSelectAction(helicopterGroup());

    pushNextStep([this]() { return abortCheck(); },
                 [this]() { return state().hasActionPoint(); },
                 [this]() { state().setMoveAction(ifvGroup().m_center - helicopterGroup().m_center); return true; },
                 "helicopters: defend tank move");

    return true;
}

GoalDefendIfv::GoalDefendIfv(State& strategyState)
    : Goal(strategyState)
    , m_helicopterIteration(std::min(strategyState.constants().m_helicoprerRadius, strategyState.game()->getHelicopterSpeed()) / 2)
{
    
    Callback abortCheckFn = [this]() { return abortCheck(); };
    Callback hasActionPointFn = [this]() { return state().hasActionPoint(); };
    Callback canMoveHelicopters = [this]()
    {
//        int conflictTicksLeft = m_lastConflictTick == 0 ? -1 : std::max(0, state().world()->getTickIndex() - m_lastConflictTick - MAX_RESOLVE_CONFLICT_TICKS);

        bool isPathFree = helicopterGroup().isPathFree(tankGroup().m_center, Obstacle(fighterGroup()), m_helicopterIteration);

        return state().hasActionPoint() && (isPathFree /*|| conflictTicksLeft == 0*/);
    };

    //pushBackStep(abortCheckFn, hasActionPointFn, [this]() { return resolveFightersHelicoptersConflict(); }, "defend tank: ensure no conflicts");
    pushBackStep(abortCheckFn, hasActionPointFn, [this]() { return shiftAircraft(); }, "defend ifv: shift aircraft");
    pushBackStep(abortCheckFn, canMoveHelicopters, [this]() { return moveHelicopters(); }, "defend ifv: move helicopters");
    

}


GoalDefendIfv::~GoalDefendIfv()
{
}
