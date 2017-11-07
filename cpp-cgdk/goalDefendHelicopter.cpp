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
    return state.m_teammates[model::VEHICLE_IFV];
}

const VehicleGroup& goals::DefendHelicopters::helicopterGroup(State& state)
{
    return state.m_teammates[model::VEHICLE_HELICOPTER];
}

goals::DefendHelicopters::DefendHelicopters(State& state)
{
    auto abortCheck = [](State& state) 
    { 
        assert(!state.m_isMoveComitted);   // conflict in move logic!
        return state.m_world->getTickIndex() > MAX_DEFEND_TICK && !state.m_isMoveComitted;
    };

    auto shouldProceed     = [](State& state) { return state.m_player->getRemainingActionCooldownTicks() == 0; };

    auto selectHelicopters = [](State& state) 
    { 
        const Rect rect = helicopterGroup(state).m_rect;

        state.m_move->setAction(ActionType::ACTION_CLEAR_AND_SELECT);
        state.m_move->setVehicleType(VehicleType::VEHICLE_HELICOPTER);
        
        state.m_move->setTop(rect.m_topLeft.m_y);
        state.m_move->setLeft(rect.m_topLeft.m_x);
        state.m_move->setBottom(rect.m_bottomRight.m_y);
        state.m_move->setRight(rect.m_bottomRight.m_x);

        state.m_isMoveComitted = true;
        return true;
    };

    auto moveToJoinPoint = [](State& state)
    {
        const Point joinPoint  = ifvGroup(state).m_center;
        const Point selfCenter = helicopterGroup(state).m_center;

        Point  path       = joinPoint - selfCenter;
        double distanceTo = selfCenter.getDistanceTo(joinPoint);
        double eta        = distanceTo / state.m_game->getHelicopterSpeed();   // TODO : correct and use prediction

        state.m_move->setAction(ACTION_MOVE);
        state.m_move->setX(path.m_x);
        state.m_move->setY(path.m_y);
        state.m_isMoveComitted = true;
        return true;
    };

    addStep(abortCheck, shouldProceed, selectHelicopters, "select helicopters");
    addStep(abortCheck, shouldProceed, moveToJoinPoint, "move helicopters to IFV");
}



