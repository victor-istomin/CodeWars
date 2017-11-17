#pragma once
#include "goal.h"
#include "forwardDeclarations.h"

namespace goals
{
    class MixTanksAndHealers : public Goal
    {
        bool shiftIfv();

        struct NeverAbort { bool operator()() { return false; } };

    public:
        MixTanksAndHealers(State& state);
        ~MixTanksAndHealers();
    };

}

