#include "DebugOut.h"
#include "RewindClient.h"
#include "model/Vehicle.h"

using namespace model;

DebugOut::DebugOut()
{
#ifdef VISUALIZER
    RewindClient::instance();   // init
#endif // VISUALIZER
}


DebugOut::~DebugOut()
{
}

void DebugOut::drawVehicles(const State::VehicleByID& vehicles, const model::Player& me)
{
#ifdef VISUALIZER
    for(const std::pair<State::Id, VehiclePtr>& idVehiclePair : vehicles)
    {
        const VehiclePtr& vehicle = idVehiclePair.second;

        uint32_t color = 0; 
        switch (vehicle->getType())
        {
        case VehicleType::TANK:
            color = RewindClient::rgba(0xFF, 0, 0);
            break;

        case VehicleType::HELICOPTER:
            color = RewindClient::rgba(0xFF, 0xFF, 0);
            break;

        case VehicleType::FIGHTER:
            color = RewindClient::rgba(0xFF, 0x50, 0x50);
            break;

        case VehicleType::IFV:
            color = RewindClient::rgba(0xFF, 0x60, 0x20);
            break;

        case VehicleType::ARRV:
            color = RewindClient::rgba(0x90, 0x50, 0);

        default:
            break;
        }

        if(vehicle->getPlayerId() != me.getId())
        {
            color |= RewindClient::rgba(0, 0, 0x80);
            color &= RewindClient::rgba(0x7F, 0xFF, 0xFF, 0xFF);
        }

        color |= RewindClient::rgba(0, 0, 0, 0xC0);

        size_t layer = vehicle->isAerial() ? 4 : 3;

        RewindClient::instance().circle(vehicle->getX(), vehicle->getY(), vehicle->getRadius(), color, layer);
    }
#endif // VISUALIZER
}

void DebugOut::commitFrame()
{
#ifdef VISUALIZER
    RewindClient::instance().end_frame();
#endif // VISUALIZER
}
