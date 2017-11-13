#pragma once
#include <memory>
#include <functional>
#include <list>
#include "forwardDeclarations.h"
#include "state.h"

class Goal
{
protected:
    typedef std::function<bool()> Callback;

private:
    struct Step;
    typedef std::unique_ptr<Step> StepPtr;

    struct Step
    {
        const char* m_debugName;

        Callback    m_shouldAbort;
        Callback    m_shouldProceed;
        Callback    m_proceed;

        Step(Callback shouldAbort, Callback shouldProceed, Callback proceed, const char* debugName = nullptr)
            : m_shouldAbort(shouldAbort), m_shouldProceed(shouldProceed), m_proceed(proceed), m_debugName(debugName) {}
    };

    std::list<StepPtr> m_steps;
    State&             m_state;

    void abortGoal() { m_steps.clear(); }

protected:

    template <typename... Args>
    void pushBackStep(Args&&... args)
    {
        return m_steps.emplace_back(std::make_unique<Step>(std::forward<Args>(args)...));
    }

    template <typename... Args>
    void pushNextStep(Args&&... args)
    {
        if (m_steps.empty())
            pushBackStep(std::forward<Args>(args)...);
        else
            m_steps.emplace(++m_steps.begin(), std::make_unique<Step>(std::forward<Args>(args)...));
    }

    State& state()             { return m_state; }
    const State& state() const { return m_state; }

    const VehicleGroup& ifvGroup()          const { return m_state.teammates(model::VehicleType::IFV); }
    const VehicleGroup& tankGroup()         const { return m_state.teammates(model::VehicleType::TANK); }
    const VehicleGroup& helicopterGroup()   const { return m_state.teammates(model::VehicleType::HELICOPTER); }
    const VehicleGroup& fighterGroup()      const { return m_state.teammates(model::VehicleType::FIGHTER); }
    const VehicleGroup& allienFighters()    const { return m_state.alliens(model::VehicleType::FIGHTER); }
    const VehicleGroup& allienHelicopters() const { return m_state.alliens(model::VehicleType::HELICOPTER); }
    const VehicleGroup& allienTanks()       const { return m_state.alliens(model::VehicleType::TANK); }

    struct WaitSomeTicks
    {
        int m_ticksRemaining;

        bool operator()() { return m_ticksRemaining-- <= 0; }
    };


public:

    Goal(State& state) : m_state(state)  {}
    virtual ~Goal()                      {}

    bool inProgress() { return !m_steps.empty(); }

    void performStep()
    {
        if (checkNuclearLaunch())
            return;

        if (!inProgress())
            return;

        const StepPtr& currentStep = m_steps.front();
        if (currentStep->m_shouldAbort())
        {
            abortGoal();
            return;
        }

        if (currentStep->m_shouldProceed())
        {
            if (currentStep->m_proceed())
            {
                m_steps.pop_front();

                // proceed with next step is this one just finished without move
                if (!m_state.isMoveCommitted())
                    performStep();
            }
            else
            {
                abortGoal();
                return;
            }
        }
    }

    bool checkNuclearLaunch()
    {
        static const int MIN_TICK_INDEX = 500;

        if (!m_state.isMoveCommitted() 
            && m_state.world()->getTickIndex() >= MIN_TICK_INDEX 
            && m_state.player()->getRemainingNuclearStrikeCooldownTicks() == 0)
        {
            const auto& allVehicles = state().getAllVehicles();

            std::map<double, VehiclePtr> nukeDamageMap;

            std::vector<VehiclePtr> teammates;
            std::vector<VehiclePtr> alliens;
            teammates.reserve(allVehicles.size());
            alliens.reserve(allVehicles.size());
            
            for(const std::pair<long long, VehiclePtr>& vehicle : allVehicles)
            {
                if (vehicle.second->getPlayerId() == state().player()->getId())
                    teammates.push_back(vehicle.second);
                else
                    alliens.push_back(vehicle.second);
            }

            static const double MIN_HEALTH = 0.5 * 100;    // TODO - remove hardcode

            for (const VehiclePtr& teammate : teammates)
            {
                if (teammate->getDurability() < MIN_HEALTH)
                    continue;   // teammate is about to go :(

                double damage = 0;
                for (const VehiclePtr& enemy : alliens)
                    if (teammate->getSquaredDistanceTo(*enemy) < teammate->getSquaredVisionRange())
                        damage += enemy->getDurability() * (enemy->getDurability() > MIN_HEALTH ? 1 : 2);

                nukeDamageMap[damage] = teammate;
            }

            struct DamageInfo
            {
                Point      m_point;
                double     m_damage;
                VehiclePtr m_guide;

                DamageInfo(const Point& p, double damage, const VehiclePtr& guide) : m_point(p), m_damage(damage), m_guide(guide) {}
            };

            static const int LOOKUP_ITEMS_LIMIT = 10;
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

                DamageInfo bestDamage { Point(), 0, teammate };

                double visionRange = teammate->getVisionRange() * 0.6;
                double squaredVR = visionRange * visionRange;
                for (const VehiclePtr& hitPoint : alliens)
                {
                    if (teammate->getSquaredDistanceTo(*hitPoint) > squaredVR)         // todo - check terrain type!
                        continue;

                    double damage = 0;
                    for (const VehiclePtr& enemy : alliens)
                        damage += getDamage(*hitPoint, *enemy);

                    for (const VehiclePtr& friendly : teammates)
                        damage += getDamage(*hitPoint, *friendly);

                    if (bestDamage.m_damage < damage)
                    {
                        bestDamage.m_damage = damage;
                        bestDamage.m_point  = *hitPoint;
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
};

