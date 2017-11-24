#pragma once
#include "noReleaseAssert.h"

#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <utility>
#include <memory>

#include "model/World.h"
#include "model/Player.h"
#include "model/Move.h"
#include "model/ActionType.h"
#include "model/Game.h"

#include "geometry.h"
#include "VehicleGroup.h"



class State
{
public:
    typedef decltype(((model::Vehicle*)nullptr)->getId()) Id;
    typedef std::map<Id, VehiclePtr>                      VehicleByID;
    typedef std::map<model::VehicleType, VehicleGroup>    GroupByType;
    typedef std::vector<Id>                               IdList;

private:

    VehicleByID   m_vehicles;
    IdList        m_selection;
    GroupByType   m_alliens;
    GroupByType   m_teammates;
    bool          m_isMoveCommitted;

    const VehicleGroup* m_nuclearGuide;

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

    State() : m_isMoveCommitted(false), m_nuclearGuide(nullptr) {}

    Constants& constants() { return *m_constants; }

    bool                  hasVehicleById(Id id) const { return m_vehicles.find(id) != m_vehicles.end(); }
    const model::Vehicle& vehicleById(Id id)    const { return *m_vehicles.find(id)->second; }
    model::Vehicle&       vehicleById(Id id)          { return *m_vehicles.find(id)->second; }

    const VehicleByID&    getAllVehicles() const      { return m_vehicles; }

    const model::World*  world()    const { return m_world; };
    const model::Player* player()   const { return m_player; };
    const model::Game*   game()     const { return m_game; };

	const GroupByType&  teammates() const                        { return m_teammates; }
	const VehicleGroup& teammates(model::VehicleType type) const { return m_teammates.find(type)->second; }
	const VehicleGroup& alliens(model::VehicleType type)   const { return m_alliens.find(type)->second; }

    bool isMoveCommitted() const                                 { return m_isMoveCommitted; }
    bool hasActionPoint() const                                  { return player()->getRemainingActionCooldownTicks() == 0; }
    bool isCorrectPosition(const Point& p) const                 { return p.m_x >= 0 && p.m_y >= 0 && p.m_x <= m_game->getWorldWidth() && p.m_y <= m_game->getWorldHeight();}
    bool isCorrectPosition(const Rect& r) const                  { return isCorrectPosition(r.m_topLeft) && isCorrectPosition(r.m_bottomRight); }


    void update(const model::World& world, const model::Player& me, const model::Game& game, model::Move& move);
    
	// actions

    void setSelectAction(const Rect& rect, model::VehicleType vehicleType = model::VehicleType::_UNKNOWN_);
    void setSelectAction(const VehicleGroup& group);
    void setAddToSelectionAction(const Rect& rect, model::VehicleType vehicleType = model::VehicleType::_UNKNOWN_);

    void setMoveAction(const Vec2d& vector);
    void setNukeAction(const Point& point, const model::Vehicle& guide);
    void setScaleAction(double factor, const Point& center);
};

