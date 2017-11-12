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

    void checkNuclearLaunch()
    {
        if (!m_state.isMoveCommitted() && m_state.player()->getRemainingNuclearStrikeCooldownTicks() == 0)
        {
            const auto& allVehicles = state().getAllVehicles();

            std::map<long long, double> nukeDamageMap;

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

                nukeDamageMap[teammate->getId()] = damage;
            }


        }
    }
};

