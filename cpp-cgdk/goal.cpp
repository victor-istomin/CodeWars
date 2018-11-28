#include "goal.h"
#include "goalManager.h"
#include "PotentialField.h"
#include "DebugOut.h"
#include <array>

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

    // #todo - remove hack
    static int s_lastCheckIndex = 0;
    if(s_lastCheckIndex == state().world()->getTickIndex())
        return false;
    s_lastCheckIndex = state().world()->getTickIndex();

    if (!m_state.isMoveCommitted()
        && m_state.getDistanceToAlliensRect() < LOOKUP_RANGE
        && m_state.player()->getRemainingNuclearStrikeCooldownTicks() == 0)
    {
        const auto& allVehicles = state().getAllVehicles();

        Rect reachableRect = state().getTeammatesRect().inflate(LOOKUP_RANGE);
        reachableRect.m_topLeft.m_x     = std::max(0.0, reachableRect.m_topLeft.m_x);
        reachableRect.m_topLeft.m_y     = std::max(0.0, reachableRect.m_topLeft.m_y);
        reachableRect.m_bottomRight.m_x = std::min(state().world()->getWidth(),  reachableRect.m_bottomRight.m_x);
        reachableRect.m_bottomRight.m_y = std::min(state().world()->getHeight(), reachableRect.m_bottomRight.m_y);

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

        auto drawDamage = [](size_t layer, Rect rect, const DamageField& field)
        {
            DebugOut debug;

            debug.drawPotentialField(rect.m_topLeft, field, layer,
                [](uint16_t damage)
            {
                uint8_t value = std::min(damage / 32, 0xFF);

                int rb = std::max(0, 0x88 - value);
                int a = std::max(0x78, 0xD8 - (value * 0x50 / 0xFF));

                auto color = damage ? RewindClient::rgba(rb, 0xFF, rb, a) : RewindClient::rgba(0x55, 0x55, 0x55, 0x88);
                if(damage == 1)
                    color = RewindClient::rgba(0xFF, 0xFF, 0xFF, 0x88);

                return color;
            });
        };


        // fill reachable spots with positive values
        DamageField damageField = getDamageField(reachableRect, teammates, teammatesHighHp, reachableAlliens);

        constexpr double MIN_DAMAGE = 200;
        constexpr size_t MAX_CELLS  = 10;
        auto bestScores = damageField.getBestN<MAX_CELLS>();

        std::vector<DamageField::Cell> bestCells;
        std::copy_if(bestScores.begin(), bestScores.end(), std::back_inserter(bestCells), [MIN_DAMAGE](const auto& cell) {return cell.score >= MIN_DAMAGE; });

        if(bestCells.empty())
            return false;




        // second iteration
        // #todo - filter out unreachable enemies

        auto cellToRect = [](const DamageField& damageField, const DamageField::Cell& cell)
        {
            return Rect{ damageField.cellTopLeftToWorld(cell.index), damageField.cellTopLeftToWorld(cell.index)
                                                                     + Point { (double)damageField.cellWidth(), (double)damageField.cellHeight() } };
        };

        // #todo - looks ugly
        drawDamage(1, reachableRect, damageField);
        auto optimizedField = std::make_unique<DamageField>(std::move(damageField));

        for(int i = 0; i < 3; ++i)
        {
            Rect optimizedRect = cellToRect(*optimizedField, bestCells.front());
            for(const auto& cell : bestCells)
            {
                optimizedRect.ensureContains(cellToRect(*optimizedField, cell));
            }

            // avoid rounding errors
            bool isTiny = (optimizedRect.width() < DRAFT_CELLS_COUNT || optimizedRect.height() < DRAFT_CELLS_COUNT);
            if(isTiny)
            {
                optimizedRect.ensureContains(Rect{ optimizedRect.m_topLeft,
                                                   optimizedRect.m_topLeft + Point {(double)DRAFT_CELLS_COUNT, (double)DRAFT_CELLS_COUNT} });
            }

            //#todo - std::move()?
            optimizedField = std::make_unique<DamageField>(getDamageField(optimizedRect, teammates, teammatesHighHp, reachableAlliens));

            auto bests = optimizedField->getBestN<MAX_CELLS / 2>();
            bestCells.clear();
            std::copy_if(bests.begin(), bests.end(), std::back_inserter(bestCells), [MIN_DAMAGE](const auto& cell) {return cell.score >= MIN_DAMAGE; });
            if(bestCells.empty())
                return false;

            if(isTiny)
                break;
        }

        /* *** debug only */
        Rect optimizedRect = cellToRect(*optimizedField, bestCells.front());
        for(const auto& cell : bestCells)
        {
            optimizedRect.ensureContains(cellToRect(*optimizedField, cell));
        }

        /*****/


        drawDamage(2, optimizedRect, *optimizedField);
        if(bestCells.empty())
            return false;

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

        DebugOut debug;
//         debug.drawRect(optimizedRect, RewindClient::rgba(0xFF, 0, 0, 0x20));

        if (!targets.empty())
        {
            state().setNukeAction(targets.front().m_point, *targets.front().m_guide);
            debug.drawNuke(targets.front().m_point, targets.front().m_guide, m_state);
        }

        /**/
        debug.drawVehicles(state().getAllVehicles(), *state().player());
        debug.commitFrame();
        /**/

    }

    return m_state.isMoveCommitted();
}

Goal::DamageField Goal::getDamageField(const Rect& reachableRect, const std::vector<VehiclePtr>& teammates, const std::vector<VehiclePtr>& teammatesHighHp, const std::vector<VehiclePtr>& reachableAlliens)
{
    clock_t dbg_startTime = clock();
    DamageField affectEnemyField ((int)reachableRect.width(), (int)reachableRect.height(), reachableRect.m_topLeft);

    Point fieldsDxDy        = reachableRect.m_topLeft;
    const double cellHypot  = std::hypot(affectEnemyField.cellWidth() / 2, affectEnemyField.cellHeight() / 2);
    const double nukeRadius = m_state.game()->getTacticalNuclearStrikeRadius() + cellHypot;

    // fill each reachable by teammate cell with '1'
    affectEnemyField.apply(teammatesHighHp,
        [nukeRadius](const VehiclePtr& teammate, uint16_t isReachable, const Point& cellCenter, const auto& pf)
    {
        if(isReachable)
            return isReachable;

        double maxDistance = teammate->getVisionRange() + nukeRadius;
        double maxSquare = maxDistance * maxDistance;

        return static_cast<uint16_t>((cellCenter.getSquareDistance(*teammate) < maxSquare) ? 1 : 0);
    });

    auto pow2 = [](auto x) { return x * x; };
    const double nukeRadiusSqare = pow2(nukeRadius);
    const double maxDamage = m_state.game()->getMaxTacticalNuclearStrikeDamage();

    // fill each cell which can affect enemy
    affectEnemyField.apply(reachableAlliens,
        [&nukeRadiusSqare, &maxDamage, &nukeRadius/**/]
    (const VehiclePtr& enemy, uint16_t score, const Point& cellCenter, const auto& dummy)
    {
        if(score == 0)
            return score;   // cell is not reachable by teammates

        double distanceSquare = cellCenter.getSquareDistance(*enemy);
        if(distanceSquare >= nukeRadiusSqare)
            return score;

		double thisDamage = (nukeRadius - sqrt(distanceSquare)) * maxDamage / nukeRadius;

        int newScore = score + static_cast<int>(thisDamage);
        if(newScore > std::numeric_limits<decltype(score)>::max())
            newScore = std::numeric_limits<decltype(score)>::max();

        return static_cast<decltype(score)>(newScore);
    });

    // add a penalty for friendly-fire
    affectEnemyField.apply(teammates,
        [&nukeRadiusSqare, &maxDamage, &nukeRadius/**/](const VehiclePtr& teammate, uint16_t score, const Point& cellCenter, const auto& pf)
    {
        if(score <= 1)
            return score;    // no sense to drop nuke here

        double distanceSquare = cellCenter.getSquareDistance(*teammate);
        if(distanceSquare >= nukeRadiusSqare)
            return score;

        constexpr int PENALTY = -2;
		double thisDamage = PENALTY * (nukeRadius - sqrt(distanceSquare)) * maxDamage / nukeRadius;

        int newScore = score + static_cast<int>(thisDamage);
        if(newScore < 1)
            newScore = 0;

        return static_cast<decltype(score)>(newScore);
    });

    DebugTimer::instance().addEvent(__FUNCTION__, clock() - dbg_startTime);

    return affectEnemyField;
}
