#pragma once
#include "noReleaseAssert.h"

#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <utility>
#include <memory>
#include <type_traits>
#include <unordered_map>

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
    typedef std::unordered_map<Id, Vec2d>                 SpeedById;
    typedef std::map<model::VehicleType, VehicleGroup>    GroupByType;
    typedef std::vector<Id>                               IdList;

	struct EnemyStrategyStats
	{
		int m_startedWithAirRush  = 0;     // tried to start with rushing me with aircraft
		int m_startedWithSlowHeap = 0;     // tried to make a heap with all units and slowly go to my base
	};


private:

    VehicleByID   m_vehicles;
    SpeedById     m_vehicleSpeed;
    IdList        m_selection;
    GroupByType   m_alliens;
    GroupByType   m_teammates;
    bool          m_isMoveCommitted;

    Rect m_teammatesRect;
    Rect m_alliensRect;

    const VehicleGroup* m_nuclearGuide;

    const model::World*  m_world;
    const model::Player* m_player;
    const model::Game*   m_game;
    model::Move*         m_move;

    struct Constants;
    std::unique_ptr<Constants> m_constants;
	EnemyStrategyStats   m_enemyStats;

    static void updateGroupsRect(const GroupByType& groupsMap, Rect& rect);


    struct Constants
    {
        struct PointInt
        { 
            int m_x; 
            int m_y; 

            PointInt(int x, int y) : m_x(x), m_y(y) {}
        };

        typedef std::map<model::TerrainType, double> GroundVisibility;
        typedef std::map<model::WeatherType, double> AirVisibility;
        typedef std::map<model::VehicleType, double> UnitVisionRadius;

        typedef std::remove_reference_t<decltype(static_cast<model::World*>(nullptr)->getTerrainByCellXY())> TerrainCells;
        typedef std::remove_reference_t<decltype(static_cast<model::World*>(nullptr)->getWeatherByCellXY())> WeatherCells;

        double           m_helicoprerRadius;
        GroundVisibility m_groundVisibility;
        AirVisibility    m_airVisibility;
        UnitVisionRadius m_unitVision;
        TerrainCells     m_terrain;
        WeatherCells     m_weather;
        const PointInt   m_tileSize;

        PointInt getTileIndex(const model::Vehicle& v) const;
        model::WeatherType getWeather(const PointInt& tile) const;
        model::TerrainType getTerrain(const PointInt& tile) const;

        double getMaxVisionRange(model::VehicleType type) const;
        double getVisionFactor(model::WeatherType weather) const;
        double getVisionFactor(model::TerrainType terrain) const;

        Constants(double helicopterRadius, const PointInt& tileSize, 
                  GroundVisibility&& groundVisibility, AirVisibility&& airVisibility, UnitVisionRadius&& unitVision,
                  const TerrainCells& terrain, const WeatherCells& weather)
            : m_helicoprerRadius(helicopterRadius), m_tileSize(tileSize)
            , m_groundVisibility(groundVisibility), m_airVisibility(airVisibility)
            , m_unitVision(unitVision), m_terrain(terrain), m_weather(weather)
        {}
    };

    void updateSelection();
    void initConstants();
    void updateNuclearGuide();
    void updateVehicles();
	void updateEnemyStats();

public:

    // TODO: move somewhere:
    enum
    {
        GROUP_ARRV_EVEN = 1,
    };

    State() : m_world(nullptr), m_game(nullptr), m_move(nullptr), m_player(nullptr)
            , m_isMoveCommitted(false), m_nuclearGuide(nullptr) 
    {}

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

    double getUnitVisionRange(const model::Vehicle& v) const;

    bool isEnemyCoveredByAnother(model::VehicleType groupId, VehicleGroup& mergedGroups) const;

	const Rect& getTeammatesRect() const                        { return m_teammatesRect; }
	double getDistanceToAlliensRect() const;


    void update(const model::World& world, const model::Player& me, const model::Game& game, model::Move& move);

    void updateGroups();

    // actions

    void setSelectAction(const Rect& rect, model::VehicleType vehicleType = model::VehicleType::_UNKNOWN_);
    void setAddSelectionAction(const Rect& rect, model::VehicleType vehicleType = model::VehicleType::_UNKNOWN_);
    void setSelectAction(const VehicleGroup& group);
    void setSelectAction(int groupId);
    void setDeselectAction(const Rect& rect, model::VehicleType vehicleType = model::VehicleType::_UNKNOWN_);
    void setAssignGroupAction(int groupNumber);

    void setMoveAction(const Vec2d& vector);
    void setNukeAction(const Point& point, const model::Vehicle& guide);
    void setScaleAction(double factor, const Point& center);
};

