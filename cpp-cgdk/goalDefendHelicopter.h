#pragma once
#include <memory>
#include "goal.h"
#include "forwardDeclarations.h"


namespace goals
{
    class DefendHelicopters : public Goal
    {
        static const VehicleGroup& ifvGroup(State& state);
        static const VehicleGroup& helicopterGroup(State& state);
		static const VehicleGroup& fighterGroup(State& state);

    public:
        DefendHelicopters(State& state);
        
    };
}

