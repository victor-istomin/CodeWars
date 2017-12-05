#include <list>
#include <algorithm>

#include "GoalRushWithAircraft.h"
#include "goalUtils.h"
#include "noReleaseAssert.h"

using namespace goals;
using namespace model;


RushWithAircraft::RushWithAircraft(State& worldState, GoalManager& goalManager)
    : Goal(worldState, goalManager)
{
    // TODO - resolve collision with helicopters before start

    pushBackStep([this]() { return shouldAbort(); },
                 [this]() { return state().hasActionPoint(); },
                 [this]() { return doNextFightersMove(); },
                 "rush air: first fighters step");
}


RushWithAircraft::~RushWithAircraft()
{
}

bool RushWithAircraft::shouldAbort() const
{
    return fighterGroup().m_units.empty();
}

bool RushWithAircraft::doNextFightersMove()
{
    const VehicleGroup& fighters = fighterGroup();
    if (fighters.m_units.empty())
        return false;  // KIA

    TargetInfo bestTargetInfo = getFightersTargetInfo();
    const VehiclePtr firstFighter = fighters.m_units.front().lock();
    const VehiclePtr firstEnemy   = bestTargetInfo.m_group->m_units.empty() ? VehiclePtr() : bestTargetInfo.m_group->m_units.front().lock();

    if (bestTargetInfo.isEliminated() || !firstEnemy)
        return true;  // nothing to attack

    const double near = state().game()->getFighterAerialAttackRange() - 2 * state().game()->getVehicleRadius();
    const double far  = state().getUnitVisionRange(*firstFighter);

    const double healthFactor     = fighters.m_healthSum / bestTargetInfo.m_group->m_healthSum;
    const double fightersStrength = std::max(0, firstFighter->getAerialDamage() - firstEnemy->getAerialDefence()) * healthFactor;

    static const double k_minDanger = 0.01;
    double desiredDistance = fightersStrength > bestTargetInfo.m_dangerFactor || bestTargetInfo.m_dangerFactor < k_minDanger ? near : far;
    if (!isAerial(bestTargetInfo.m_type))
    {
        desiredDistance = far;   // force far distance, because fighter can't attack them. TODO: maybe, overlap with enemy between nuke attacks?
    }

    /*** temporary code: replace this by something more sophisticated */
    //desiredDistance += (fighters.m_rect.height() + fighters.m_rect.width()) / 4; 
    /*****/

    struct Nearest 
    { 
        Point  m_position       = Point(); 
        double m_squareDistance = std::numeric_limits<double>::max(); 

        void apply(const Point& candidate, double candidateSqDistance)
        {
            if (candidateSqDistance < m_squareDistance)
            {
                m_position       = candidate;
                m_squareDistance = candidateSqDistance;
            }
        }
    };

    Nearest target;
    std::for_each(bestTargetInfo.m_group->m_units.begin(), bestTargetInfo.m_group->m_units.end(), 
        [&target, &fighters](const VehicleCache& unitCache) 
    { 
        VehiclePtr unit = unitCache.lock(); 
        target.apply(*unit, fighters.m_center.getSquareDistance(*unit));
    });

    Vec2d reverseDirection = Vec2d(fighters.m_center - target.m_position).truncate(desiredDistance);
    Point attackPosition   = target.m_position + reverseDirection;
    Vec2d moveVector       = attackPosition - fighters.m_center;    // TODO - unit-perfect aim

    bool isMoveAllowed = validateMoveVector(moveVector);  // don't retreat in case of guiding nuclear launch

    state().setSelectAction(fighters);

    static const int MIN_TICKS_TO_WAIT = 10;
    int ticksToWait = isMoveAllowed ? std::max(MIN_TICKS_TO_WAIT, static_cast<int>(moveVector.length() / (firstFighter->getMaxSpeed() * 2)))
                                    : std::max(1, state().player()->getNextNuclearStrikeTickIndex() - state().world()->getTickIndex());

    // LIFO push/pop order!
    // move to attack point, wait some ticks and repeat

    pushNextStep([this]() { return shouldAbort(); },
                 [this]() { return state().hasActionPoint(); },
                 [this]() { return doNextFightersMove(); },
                 "doNextFightersMove");

    pushNextStep([this]() { return shouldAbort(); }, WaitSomeTicks{ state(), ticksToWait }, DoNothing(), "wait next step", StepType::ALLOW_MULTITASK);

    if (isMoveAllowed)
    {
        pushNextStep([this]() { return shouldAbort(); }, 
                     [this]() { return state().hasActionPoint(); }, 
                     [this, moveVector]() { state().setMoveAction(moveVector); return true; },
                     "fighters rush");
    }

    return true;
}

RushWithAircraft::TargetInfo RushWithAircraft::getFightersTargetInfo()
{
    const VehicleGroup& fighters = fighterGroup();

    // for now, no sense to attack ARRV with nuke, because they're healing oneself
    static const VehicleType targetPriority[] = { VehicleType::HELICOPTER, VehicleType::FIGHTER, VehicleType::TANK, VehicleType::IFV };

    std::list<TargetInfo> targets;
    for (VehicleType type : targetPriority)
        targets.emplace_back(state().alliens(type));

    targets.remove_if([](const TargetInfo& ti) { return ti.isEliminated(); });

    VehicleType  dangerous[] = { VehicleType::FIGHTER, VehicleType::HELICOPTER, VehicleType::IFV };

    const double myDefense   = state().game()->getFighterAerialDefence();
    const Point  myCorners[] = { fighters.m_rect.m_topLeft, fighters.m_rect.m_bottomRight, fighters.m_rect.topRight(), fighters.m_rect.bottomLeft() };

    for (TargetInfo& target : targets)
    {
        assert(target.m_type != VehicleType::_UNKNOWN_);

        const VehicleGroup& targetGroup  = *target.m_group;
        const double        damage       = std::max(0.0, targetGroup.m_units.front().lock()->getAerialDamage() - myDefense);
        const double        healthFactor = targetGroup.m_healthSum / fighters.m_healthSum;

        target.m_dangerFactor = damage * healthFactor;

        // compute protection

        for (VehicleType defenderType : dangerous)
        {
            const VehicleGroup& defender = state().alliens(defenderType);
            if (target.m_type != defenderType && !defender.m_units.empty() && targetGroup.m_rect.overlaps(defender.m_rect))
            {
                const double defDamage       = std::max(0.0, defender.m_units.front().lock()->getAerialDamage() - myDefense);
                const double defHealthFactor = defender.m_healthSum / fighters.m_healthSum;

                target.m_dangerFactor += defDamage * defHealthFactor;
            }
        }

        // compute min distance to my corner. Use corners here in order to avoid O(N^2)

        for (const Point& corner : myCorners)
            for (const VehicleCache& enemyCache : targetGroup.m_units)
                target.m_minSqDistance = std::min(target.m_minSqDistance, corner.getSquareDistance(*enemyCache.lock()));
    }

    // targets already sorted by priority, stable sort by danger factor...
    targets.sort([](const TargetInfo& a, const TargetInfo& b) { return a.m_dangerFactor < b.m_dangerFactor; });

    // and then by min distance
    targets.sort([](const TargetInfo& a, const TargetInfo& b) { return a.m_minSqDistance < b.m_minSqDistance; });

    return targets.empty() ? TargetInfo(allienFighters()) : targets.front();   // in case of empty targets list, returns fake target with empty eliminated flag
}

bool RushWithAircraft::validateMoveVector(Vec2d& moveVector)
{
    const VehicleGroup& fighters = fighterGroup();

    bool isMoveAllowed = true;                                   // don't retreat in case of guiding nuclear launch
    VehiclePtr nuclearGuide = state().nuclearGuideUnit();
    if (state().nuclearGuideGroup() == &fighters && nuclearGuide != nullptr)
    {
        Point nukePoint = state().nuclearMissileTarget();
        Point nextGuidePoint = Point(*nuclearGuide) + moveVector;
        assert(nukePoint != Point());

        double nextHighlightDistance = nextGuidePoint.getDistanceTo(nukePoint);
        double nextVisionRange = state().getUnitVisionRangeAt(*nuclearGuide, nextGuidePoint);

        isMoveAllowed = nextVisionRange >= nextHighlightDistance;
    }

    static const double MIN_STEP = state().game()->getFighterSpeed() / 8;
    if (!isMoveAllowed && moveVector.length() > MIN_STEP)
    {
        // try adjust move before denying
        moveVector /= 2;
        return validateMoveVector(moveVector);
    }

    return isMoveAllowed;
}
