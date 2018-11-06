#include "goal.h"
#include "goalManager.h"
#include "PotentialField.h"
#include "DebugOut.h"

#undef min
#undef max

void Goal::performStep(GoalManager& goalManager, bool isBackgroundMode)
{
    if (checkNuclearLaunch())
        return;

    if (isFinished())
        return;

    const Step* currentStep = m_steps.front().get();
    if (currentStep->m_shouldAbort())
    {
        abortGoal();
        return;
    }

    if (currentStep->m_shouldProceed())
    {
        if (currentStep->m_proceed())
        {
            m_isStarted = true;
            currentStep = nullptr;
            m_steps.pop_front();

            // proceed with next step is this one just finished without move
            if (isNoMoveComitted())
                performStep(goalManager, isBackgroundMode);
        }
        else
        {
            abortGoal();
            return;
        }
    }

    // do multitasking in background mode, if not doing yet
    if (!isBackgroundMode)
        doMultitasking(goalManager);
}

void Goal::doMultitasking(GoalManager& goalManager)
{
    if(isNoMoveComitted() && !m_steps.empty() && m_steps.front()->m_isMultitaskPoint)
    {
        // not yet ready for current step, do something else
        goalManager.doMultitasking(this);
    }
}

bool Goal::isNoMoveComitted()
{
    return !m_state.isMoveCommitted();
}

bool Goal::checkNuclearLaunch()
{
    static const double LOOKUP_RANGE = 10 * m_state.game()->getFighterSpeed() + m_state.game()->getFighterVisionRange() 
                                          + m_state.game()->getTacticalNuclearStrikeRadius();

    if (!m_state.isMoveCommitted()
        && m_state.getDistanceToAlliensRect() < LOOKUP_RANGE
        && m_state.player()->getRemainingNuclearStrikeCooldownTicks() == 0)
    {
        const auto& allVehicles = state().getAllVehicles();

        Rect reachableRect = state().getTeammatesRect().inflate(LOOKUP_RANGE);

        std::map<double, VehiclePtr> nukeDamageMap;

        std::vector<VehiclePtr> teammates;
        std::vector<VehiclePtr> teammatesHighHp;
        std::vector<VehiclePtr> reachableAlliens;
        teammates.reserve(allVehicles.size());
        teammatesHighHp.reserve(allVehicles.size());
        reachableAlliens.reserve(allVehicles.size());

        const model::Player& enemyPlayer = *state().enemy();
        Point enemyNuke = state().enemyNuclearMissileTarget();
        int   ticksToEnemyNuke = enemyPlayer.getNextNuclearStrikeTickIndex() != -1 ? enemyPlayer.getNextNuclearStrikeTickIndex() - state().world()->getTickIndex() : 0;

        const double nukeRadius        = m_state.game()->getTacticalNuclearStrikeRadius();
        const double maxPossibleDamage = m_state.game()->getMaxTacticalNuclearStrikeDamage();
        const auto   myPlayerId        = m_state.player()->getId();
        const double decaySpeed        = nukeRadius / maxPossibleDamage;

        auto getDamage = [decaySpeed, maxPossibleDamage, myPlayerId](const Point& hitPoint, const model::Vehicle& unit, double teammateDamageFactor = -1.5)
        {
            double real   = std::max(0.0, maxPossibleDamage - (hitPoint.getDistanceTo(unit) / decaySpeed));   // TODO - check!
            double damage = real >= unit.getDurability() ? unit.getMaxDurability() : real;

            return (unit.getPlayerId() == myPlayerId ? teammateDamageFactor : 1.0) * real;
        };

        for (const std::pair<long long, VehiclePtr>& idVehiclePair : allVehicles)
        {
            const VehiclePtr& vehicle = idVehiclePair.second;

            if(vehicle->getPlayerId() == state().player()->getId())
            {
                teammates.push_back(vehicle);

                // filter teammates with enough HP
                static const double MIN_HEALTH = 0.5 * 100;
                const double enemyNukeDamage = enemyNuke != Point() ? getDamage(enemyNuke, *vehicle, 1.0) + ticksToEnemyNuke / 2 : 0;
                const double healthThreshold = std::max(MIN_HEALTH, enemyNukeDamage);
                if(vehicle->getDurability() > healthThreshold)
                    teammatesHighHp.push_back(vehicle);
            }
            else if(reachableRect.contains(*vehicle))
            {
                reachableAlliens.push_back(vehicle);
            }
        }

        constexpr const int DRAFT_CELLS_COUNT = 10;
        auto alignSize = [DRAFT_CELLS_COUNT](double size) 
        { 
            return static_cast<int>(std::ceil(size / DRAFT_CELLS_COUNT)) * DRAFT_CELLS_COUNT;
        };

        // fill reachable spots with positive values
        PotentialField<uint8_t> reachabilityTeam(alignSize(reachableRect.width()), alignSize(reachableRect.height()), DRAFT_CELLS_COUNT);
        PotentialField<uint8_t> affectEnemy(alignSize(reachableRect.width()), alignSize(reachableRect.height()), DRAFT_CELLS_COUNT);
        Point fieldsDxDy = reachableRect.m_topLeft;
        
        reachabilityTeam.apply(teammatesHighHp, 
            [fieldsDxDy, nukeRadius](const VehiclePtr& teammate, uint8_t isReachable, int cellX, int cellY, const auto& pf)
        {
            if(isReachable)
                return isReachable;

            Point cellPoint = fieldsDxDy + Point(cellX, cellY);
            double maxDistance = teammate->getVisionRange() + nukeRadius + std::max(pf.cellWidth(), pf.cellHeight()) / 2;
            double maxSquare = maxDistance * maxDistance;

            return static_cast<uint8_t>( (cellPoint.getSquareDistance(*teammate) < maxSquare) ? 1 : 0 );
        });

        DebugOut debug;
//         debug.drawPotentialField(fieldsDxDy, reachabilityTeam, 0,
//             [](uint8_t isReachable) { return isReachable ? RewindClient::rgba(0xFF, 0x88, 0x88, 0xA8) : RewindClient::rgba(0xFF, 0xFF, 0xFF, 0x88); });

        double nukeRadiusSqare = nukeRadius + std::max(affectEnemy.cellWidth(), affectEnemy.cellHeight()) / 2;
        nukeRadiusSqare *= nukeRadiusSqare;

        affectEnemy.apply(reachableAlliens, 
            [fieldsDxDy, nukeRadiusSqare](const VehiclePtr& enemy, uint8_t isReachable, int cellX, int cellY, const auto& dummy)
        {
            if(isReachable)
                return isReachable;

            Point cellPoint = fieldsDxDy + Point(cellX, cellY);
            return static_cast<uint8_t>( (cellPoint.getSquareDistance(*enemy) < nukeRadiusSqare) ? 1 : 0 );
        });

//         debug.drawPotentialField(fieldsDxDy, affectEnemy, 1,
//             [](uint8_t isReachable) { return isReachable ? RewindClient::rgba(0x88, 0x88, 0xFF, 0xA8) : RewindClient::rgba(0xFF, 0xFF, 0xFF, 0x88); });

        reachabilityTeam.apply(affectEnemy, [](uint8_t isTeam, uint8_t isEnemy) { return isTeam * isEnemy; });
        debug.drawPotentialField(fieldsDxDy, reachabilityTeam, 0,
            [](uint8_t isReachable) { return isReachable ? RewindClient::rgba(0xFF, 0x88, 0x88, 0xA8) : RewindClient::rgba(0xFF, 0xFF, 0xFF, 0x88); });

        static const double MIN_DAMAGE = 90;

// 
//         damageMap.apply(teammates, [&getDamage, &enemyNuke, &fieldDxDy, ticksToEnemyNuke]
//                                    (const VehiclePtr& teammate, int cellScore, int cellX, int cellY, const PotentialField<>& field) 
//         {
//             //#todo - move outside
//             const double enemyNukeDamage = enemyNuke != Point() ? getDamage(enemyNuke, *teammate, 1.0) + ticksToEnemyNuke / 2 : 0;
//             const double healthThreshold = std::max(MIN_HEALTH, enemyNukeDamage);
//             if(teammate->getDurability() <= healthThreshold)
//                 return cellScore;   // no bonus, teammate is about to go :(
// 
//             Point cellPos = fieldDxDy + Point(cellX, cellY);
//             int bonus = cellPos.getSquareDistance(*teammate) < teammate->getSquaredVisionRange() ? 1 : 0;
//             return cellScore + bonus;
//         });

        for (const VehiclePtr& teammate : teammates)
        {
            static const double MIN_HEALTH = 0.5 * 100;   // #todo - remove duplicate

            const double enemyNukeDamage = enemyNuke != Point() ? getDamage(enemyNuke, *teammate, 1.0) + ticksToEnemyNuke / 2 : 0;
            const double healthThreshold = std::max(MIN_HEALTH, enemyNukeDamage);
            if (teammate->getDurability() <= healthThreshold)
                continue;   // teammate is about to go :(

            double damage = 0;
            double sqaredVr = teammate->getSquaredVisionRange();

            for (const VehiclePtr& enemy : reachableAlliens)
                if (teammate->getSquaredDistanceTo(*enemy) < sqaredVr)
                    damage += enemy->getDurability() * (enemy->getDurability() > MIN_HEALTH ? 1 : 2);

            if (damage >= MIN_DAMAGE)
                nukeDamageMap[damage] = teammate;
        }

        struct DamageInfo
        {
            Point      m_point;
            double     m_damage;
            VehiclePtr m_guide;

            DamageInfo(const Point& p, double damage, const VehiclePtr& guide) : m_point(p), m_damage(damage), m_guide(guide) {}
        };

        static const int LOOKUP_ITEMS_LIMIT = 50;
        std::vector<DamageInfo> targets;
        targets.reserve(LOOKUP_ITEMS_LIMIT);

        int lookupItemsLeft = LOOKUP_ITEMS_LIMIT;
        for (auto itDamage = nukeDamageMap.rbegin(); itDamage != nukeDamageMap.rend() && lookupItemsLeft > 0; ++itDamage, --lookupItemsLeft)
        {
            auto teammate = itDamage->second;

            DamageInfo bestDamage{ Point(), 0, teammate };

            // TODO - predict terrain where this unit will go next 30 ticks!

            double rangeGap = teammate->getRadius();
            double visionRange = state().getUnitVisionRange(*teammate) - 2 * teammate->getRadius() - rangeGap; 
            double squaredVR = visionRange * visionRange;

            // TODO - add hitpoints in the middle of alliens or something similar

            for (const VehiclePtr& hitPoint : reachableAlliens)
            {
                if (teammate->getSquaredDistanceTo(*hitPoint) > squaredVR)
                    continue;

                double damage = 0;
                for (const VehiclePtr& enemy : reachableAlliens)
                    damage += getDamage(*hitPoint, *enemy);

                for (const VehiclePtr& friendly : teammates)
                    damage += getDamage(*hitPoint, *friendly);

                if (bestDamage.m_damage < damage)
                {
                    bestDamage.m_damage = damage;
                    bestDamage.m_point = *hitPoint;
                }
            }

            if (bestDamage.m_damage > 0)
                targets.emplace_back(bestDamage);
        }

        // pre-sort DESC by guide durability
        std::sort(targets.begin(), targets.end(), [](const DamageInfo& a, const DamageInfo& b) { return a.m_guide->getDurability() > b.m_guide->getDurability(); });

        // sort DESC by damage
        std::stable_sort(targets.begin(), targets.end(), [](const DamageInfo& a, const DamageInfo& b) { return a.m_damage > b.m_damage; });

        if (!targets.empty())
        {
            state().setNukeAction(targets.front().m_point, *targets.front().m_guide);
        }
    }

    return m_state.isMoveCommitted();
}
