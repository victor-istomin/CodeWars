#pragma once
#include "state.h"

class DebugOut
{
public:
    DebugOut();
    ~DebugOut();

    void drawVehicles(const State::VehicleByID& vehicles, const model::Player& me);
    void commitFrame();

};

