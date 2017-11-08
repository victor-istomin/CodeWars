﻿#pragma once
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <utility>
#include <cassert>
#include <memory>

#include "model/World.h"
#include "model/Player.h"
#include "model/Move.h"
#include "model/ActionType.h"
#include "geometry.h"
#include "VehicleGroup.h"

struct State
{
    typedef decltype(((model::Vehicle*)nullptr)->getId()) Id;
    typedef std::map<Id, VehiclePtr>                      VehicleByID;
    typedef std::map<model::VehicleType, VehicleGroup>    GroupByType;

    VehicleByID m_vehicles;
    GroupByType m_alliens;
    GroupByType m_teammates;
    bool        m_isMoveComitted;

    const model::World*  m_world;
    const model::Player* m_player;
    const model::Game*   m_game;
    model::Move*         m_move;

    bool                  hasVehicleById(Id id) const { return m_vehicles.find(id) != m_vehicles.end(); }
    const model::Vehicle& vehicleById(Id id)    const { return *m_vehicles.find(id)->second; }
    model::Vehicle&       vehicleById(Id id)          { return *m_vehicles.find(id)->second; }

    void update(const model::World& world, const model::Player& me, const model::Game& game, model::Move& move)
    {
        assert(me.isMe());
        m_world  = &world;
        m_player = &me;
        m_game   = &game;
        m_move   = &move;

        m_isMoveComitted = false;

        for (const model::Vehicle& v : world.getNewVehicles())
        {
            auto newVehicle = std::make_shared<model::Vehicle>(v);
            m_vehicles[v.getId()] = newVehicle;

            if (v.getPlayerId() == me.getId())
                m_teammates[v.getType()].add(newVehicle);
            else
                m_alliens[v.getType()].add(newVehicle);
        }

        for (const model::VehicleUpdate& update : world.getVehicleUpdates())
        {
            if (update.getDurability() != 0)
                vehicleById(update.getId()) = model::Vehicle(vehicleById(update.getId()), update);
            else
                m_vehicles.erase(update.getId());
        }

        for (auto& group : m_teammates)
            group.second.update();

        for (auto& group : m_alliens)
            group.second.update();
    }
    
    void setSelectAction(const Rect& rect, model::VehicleType vehicleType = model::_VEHICLE_UNKNOWN_)
    {
        m_move->setAction(model::ACTION_CLEAR_AND_SELECT);
        
        if (vehicleType != model::_VEHICLE_UNKNOWN_)
            m_move->setVehicleType(vehicleType);
        
        m_move->setTop(rect.m_topLeft.m_y);
        m_move->setLeft(rect.m_topLeft.m_x);
        m_move->setBottom(rect.m_bottomRight.m_y);
        m_move->setRight(rect.m_bottomRight.m_x);
        
        m_isMoveComitted = true;
    }
    
    void setMoveAction(const Vec2d& vector)
    {
        m_move->setAction(model::ACTION_MOVE);
        m_move->setX(vector.m_x);
        m_move->setY(vector.m_y);
        
        m_isMoveComitted = true;
    }
};
