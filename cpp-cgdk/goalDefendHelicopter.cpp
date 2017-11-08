#include "goalDefendHelicopter.h"
#include "state.h"

#include "model/ActionType.h"
#include "model/VehicleType.h"
#include "model/Move.h"
#include "model/Game.h"

using namespace goals;
using namespace model;

namespace
{
    const int MAX_DEFEND_TICK = 100;
}

const VehicleGroup& goals::DefendHelicopters::ifvGroup(State& state)
{
    return state.teammates(model::VEHICLE_IFV);
}

const VehicleGroup& goals::DefendHelicopters::helicopterGroup(State& state)
{
    return state.teammates(model::VEHICLE_HELICOPTER);
}

goals::DefendHelicopters::DefendHelicopters(State& state)
{
    auto abortCheck = [](State& state) 
    { 
        assert(!state.isMoveCommitted());   // conflict in move logic!
        return state.world()->getTickIndex() > MAX_DEFEND_TICK && !state.isMoveCommitted();
    };

    auto shouldProceed     = [](State& state) { return state.player()->getRemainingActionCooldownTicks() == 0; };

    auto selectHelicopters = [](State& state) 
    { 
        state.setSelectAction(helicopterGroup(state).m_rect, VehicleType::VEHICLE_HELICOPTER);
        return true;
    };

    auto moveToJoinPoint = [](State& state)
    {
        const Point joinPoint  = ifvGroup(state).m_center;
        const Point selfCenter = helicopterGroup(state).m_center;

        Vec2d  path       = joinPoint - selfCenter;
        double distanceTo = selfCenter.getDistanceTo(joinPoint);
        double eta        = distanceTo / state.game()->getHelicopterSpeed();   // TODO : use correct prediction

        state.setMoveAction(path);
        return true;
    };

    addStep(abortCheck, shouldProceed, selectHelicopters, "select helicopters");
    addStep(abortCheck, shouldProceed, moveToJoinPoint, "move helicopters to IFV");
}



