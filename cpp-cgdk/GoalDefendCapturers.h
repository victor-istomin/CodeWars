#pragma once
#include "goal.h"

namespace goals
{
    class DefendCapturers 
        : public Goal
    {
        virtual bool isCompatibleWith(const Goal* interrupted) override;

        bool shouldAbort()     const { return false; }
        bool hasActionPoints() const { return state().hasActionPoint(); }

    public:
        DefendCapturers(State& state, GoalManager& goalManager);
        ~DefendCapturers();

    };
}


