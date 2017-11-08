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
        Callback    m_shouldAbort;
        Callback    m_shouldProceed;
        Callback    m_proceed;
        const char* m_debugName;
        
        Step(Callback shouldAbort, Callback shouldProceed, Callback proceed, const char* debugName = nullptr)
            : m_shouldAbort(shouldAbort), m_shouldProceed(shouldProceed), m_proceed(proceed), m_debugName(debugName) {}
    };        
        
    std::list<StepPtr> m_steps;
    
    void abortGoal() { m_steps.clear(); }
    
protected:

    template <typename... Args>
    void addStep(Args&&... args)
    {
        return m_steps.emplace_back(std::make_unique<Step>( std::forward<Args>(args)... ));
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
                if (!state.m_isMoveComitted)
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

