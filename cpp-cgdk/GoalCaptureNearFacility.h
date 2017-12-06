#pragma once
#include "goal.h"
#include "forwardDeclarations.h"

namespace goals
{
    class CaptureNearFacility 
        : public Goal
    {
    public:
        CaptureNearFacility(State& state, GoalManager& goalManager);
        ~CaptureNearFacility();
    };
}

