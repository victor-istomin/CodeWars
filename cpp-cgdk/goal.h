#pragma once
#include <memory>
#include <functional>
#include <list>
#include "forwardDeclarations.h"
#include "state.h"

class Goal
{
    struct Step;
    typedef std::function<bool(State&)> Callback;
    typedef std::unique_ptr<Step>       StepPtr;

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

public:

    bool inProgress() { return !m_steps.empty(); }

    void performStep(State& state)
    {
        if (!inProgress())
            return;

        const StepPtr& currentStep = m_steps.front();
        if (currentStep->m_shouldAbort(state))
        {
            abortGoal();
            return;
        }

        if (currentStep->m_shouldProceed(state))
        {
            if (currentStep->m_proceed(state))
            {
                m_steps.pop_front();

                // proceed with next step is this one just finished without move
                if (!state.isMoveCommitted())
                    performStep(state);
            }
            else
            {
                abortGoal();
                return;
            }
        }
    }
};

