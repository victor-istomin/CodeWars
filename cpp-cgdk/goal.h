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
        bool        m_isMultitaskPoint;

        Step(Callback shouldAbort, Callback shouldProceed, Callback proceed, const char* debugName = nullptr, bool isMultitackPoint = false)
            : m_shouldAbort(shouldAbort), m_shouldProceed(shouldProceed), m_proceed(proceed), m_debugName(debugName), m_isMultitaskPoint(isMultitackPoint) {}
    };

    std::list<StepPtr> m_steps;
    State&             m_state;

    void abortGoal() { m_steps.clear(); }

    void doMultitasking(GoalManager &goalManager);
    bool isNoMoveComitted();
    bool checkNuclearLaunch();

    // check if this goal could be performed in multitasking mode when 'interrupted' has nothing to do right now
    virtual bool isCompatibleWith(const Goal* interrupted) { return false; }

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

    State& state()                                { return m_state; }
    const State& state() const                    { return m_state; }

    const VehicleGroup& ifvGroup()          const { return m_state.teammates(model::VehicleType::IFV); }
    const VehicleGroup& tankGroup()         const { return m_state.teammates(model::VehicleType::TANK); }
    const VehicleGroup& arrvGroup()         const { return m_state.teammates(model::VehicleType::ARRV); }
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

    bool inProgress()                    { return !m_steps.empty(); }

    bool isEligibleForBackgroundMode(const Goal* interrupted) { return this != interrupted && isCompatibleWith(interrupted); }

    void performStep(GoalManager& goalManager, bool isBackgroundMode);
};

