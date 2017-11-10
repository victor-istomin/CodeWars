#pragma once
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



class State
{

    typedef decltype(((model::Vehicle*)nullptr)->getId()) Id;
    typedef std::map<Id, VehiclePtr>                      VehicleByID;
    typedef std::map<model::VehicleType, VehicleGroup>    GroupByType;

    VehicleByID m_vehicles;
    GroupByType m_alliens;
    GroupByType m_teammates;
    bool        m_isMoveCommitted;

    const model::World*  m_world;
    const model::Player* m_player;
    const model::Game*   m_game;
    model::Move*         m_move;

    struct Constants;
    std::unique_ptr<Constants> m_constants;

    struct Constants
    {
        double m_helicoprerRadius;

        Constants(double helicopterRadius) : m_helicoprerRadius(helicopterRadius) {}
    };

public:

    Constants& constants() { return *m_constants; }    

    bool                  hasVehicleById(Id id) const { return m_vehicles.find(id) != m_vehicles.end(); }
    const model::Vehicle& vehicleById(Id id)    const { return *m_vehicles.find(id)->second; }
    model::Vehicle&       vehicleById(Id id)          { return *m_vehicles.find(id)->second; }

    const model::World*  world()    const { return m_world; };
    const model::Player* player()   const { return m_player; };
    const model::Game*   game()     const { return m_game; };

	const VehicleGroup& teammates(model::VehicleType type) const { return m_teammates.find(type)->second; }
	const VehicleGroup& alliens(model::VehicleType type)   const { return m_alliens.find(type)->second; }

    bool isMoveCommitted()          const { return m_isMoveCommitted; }


    void update(const model::World& world, const model::Player& me, const model::Game& game, model::Move& move)
    {
        assert(me.isMe());
        m_world  = &world;
        m_player = &me;
        m_game   = &game;
        m_move   = &move;

        if (!m_constants)
        {
            auto itHelicopter = std::find_if(m_world->getNewVehicles().begin(), m_world->getNewVehicles().end(), 
				[](const model::Vehicle& v) { return v.getType() == model::VehicleType::HELICOPTER; });
            double helicopterRadius = itHelicopter != m_world->getNewVehicles().end() ? itHelicopter->getRadius() : 2;

            m_constants = std::make_unique<Constants>(helicopterRadius);
        }

        m_isMoveCommitted = false;

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
    
    void setSelectAction(const Rect& rect, model::VehicleType vehicleType = model::VehicleType::_UNKNOWN_)
    {
        m_move->setAction(model::ActionType::CLEAR_AND_SELECT);
        
        if (vehicleType != model::VehicleType::_UNKNOWN_)
            m_move->setVehicleType(vehicleType);
        
        m_move->setTop(rect.m_topLeft.m_y);
        m_move->setLeft(rect.m_topLeft.m_x);
        m_move->setBottom(rect.m_bottomRight.m_y);
        m_move->setRight(rect.m_bottomRight.m_x);
        
        m_isMoveCommitted = true;
    }
    
    void setMoveAction(const Vec2d& vector)
    {
        m_move->setAction(model::ActionType::MOVE);
        m_move->setX(vector.m_x);
        m_move->setY(vector.m_y);
        
        m_isMoveCommitted = true;
    }
};

