#pragma once
#include <memory>
#include <functional>
#include <list>
#include "forwardDeclarations.h"

class Goal
{
    typedef std::function<bool(State&)> Callback;
    
    struct Step
    {
        Callback    m_shouldAbort;
        Callback    m_shouldProceed;
        Callback    m_proceed;

        const char* m_debugName;
        
        Step(Callback shouldAbort, Callback shouldProceed, Callback proceed, const char* debugName = nullptr)
            : m_shouldAbort(shouldAbort), m_shouldProceed(shouldProceed), m_proceed(proceed), m_debugName(debugName) {}
    };        
        
    typedef std::unique_ptr<Step> StepPtr;
    typedef std::list<StepPtr>    Steps;
    
    Steps         m_steps;
    const StepPtr m_nullStep;
    
    void abortGoal() { m_steps.clear(); }
    
protected:

    void addStep(Callback shouldAbort, Callback shouldProceed, Callback proceed, const char* debugName)
    {
        return m_steps.emplace_back(std::make_unique<Step>(shouldAbort, shouldProceed, proceed, debugName));
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
            }
            else
            {
                abortGoal();
                return;
            }
        }
    }    
};

