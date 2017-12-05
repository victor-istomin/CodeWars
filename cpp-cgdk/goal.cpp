#include "goal.h"
#include "goalManager.h"

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
        std::vector<VehiclePtr> reachangeAlliens;
        teammates.reserve(allVehicles.size());
        reachangeAlliens.reserve(allVehicles.size());

        for (const std::pair<long long, VehiclePtr>& idVehiclePair : allVehicles)
        {
            const VehiclePtr& vehicle = idVehiclePair.second;

            if (vehicle->getPlayerId() == state().player()->getId())
                teammates.push_back(vehicle);
            else if (reachableRect.contains(*vehicle))
                reachangeAlliens.push_back(vehicle);
        }

        static const double MIN_HEALTH = 0.5 * 100;    // TODO - remove hardcode
        static const double MIN_DAMAGE = 32;

        for (const VehiclePtr& teammate : teammates)
        {
            if (teammate->getDurability() < MIN_HEALTH)
                continue;   // teammate is about to go :(

            double damage = 0;
            double sqaredVr = teammate->getSquaredVisionRange();

            for (const VehiclePtr& enemy : reachangeAlliens)
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

        const double damageRadius = m_state.game()->getTacticalNuclearStrikeRadius();
        const double maxPossibleDamage = m_state.game()->getMaxTacticalNuclearStrikeDamage();
        const auto   myPlayerId = m_state.player()->getId();

        auto getDamage = [damageRadius, maxPossibleDamage, myPlayerId](const Point& hitPoint, const model::Vehicle& enemy)
        {
            double decay = damageRadius / maxPossibleDamage;
            double real = std::max(0.0, maxPossibleDamage - (hitPoint.getDistanceTo(enemy) / decay));   // TODO - check!
            double damage = real >= enemy.getDurability() ? enemy.getMaxDurability() : real;

            return (enemy.getPlayerId() == myPlayerId ? -1.5 : 1.0) * real;
        };

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

            for (const VehiclePtr& hitPoint : reachangeAlliens)
            {
                if (teammate->getSquaredDistanceTo(*hitPoint) > squaredVR)
                    continue;

                double damage = 0;
                for (const VehiclePtr& enemy : reachangeAlliens)
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
