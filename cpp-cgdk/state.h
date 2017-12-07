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
#include "model/Facility.h"

#include "geometry.h"
#include "VehicleGroup.h"

class State
{
public:
    typedef decltype(((model::Vehicle*)nullptr)->getId()) Id;
    static const Id INVALID_ID = (Id)-1;

    typedef std::unordered_map<Id, model::Facility>       FacilityById;
    typedef std::unordered_map<Id, VehiclePtr>            VehicleByID;
    typedef std::map<model::VehicleType, VehicleGroup>    GroupByType;    // not eligible for unordered_map due to references to VehicleGroup's
    typedef std::vector<Id>                               IdList;

	struct EnemyStrategyStats
	{
        static const float MAX_SCORE;
        static const float POSITIVE_SCORE;
        static const float INCREMENT;
        static const float INCREMENT_DIVIDER;

		float m_startedWithAirRush  = 0;     // start strategy: rushing me with aircraft
		float m_startedWithSlowHeap = 0;     // start strategy: make a heap with all units and slowly go to my base

        bool isAirRush() const  { return m_startedWithAirRush  >= POSITIVE_SCORE; }
        bool isSlowHeap() const { return m_startedWithSlowHeap >= POSITIVE_SCORE; }
	};

private:

    VehicleByID   m_vehicles;
    FacilityById  m_facilities;
    IdList        m_selection;
    GroupByType   m_alliens;
    GroupByType   m_teammates;
    bool          m_isMoveCommitted;

    Rect m_teammatesRect;
    Rect m_alliensRect;

    const VehicleGroup* m_nuclearGuideGroup;

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
        typedef std::map<model::TerrainType, double> GroundMobility;
        typedef std::map<model::WeatherType, double> AirVisibility;
        typedef std::map<model::WeatherType, double> AirMobility;
        typedef std::map<model::VehicleType, double> UnitVisionRadius;

        typedef std::remove_reference_t<decltype(static_cast<model::World*>(nullptr)->getTerrainByCellXY())> TerrainCells;
        typedef std::remove_reference_t<decltype(static_cast<model::World*>(nullptr)->getWeatherByCellXY())> WeatherCells;

        double           m_helicoprerRadius;
        GroundVisibility m_groundVisibility;
        GroundMobility   m_groundMobility;
        AirVisibility    m_airVisibility;
        AirMobility      m_airMobility;
        UnitVisionRadius m_unitVision;
        TerrainCells     m_terrain;
        WeatherCells     m_weather;
        const PointInt   m_tileSize;

        static const int DEFEND_DESICION_TICK = 500;

        PointInt getTileIndex(const Point& p) const;
        model::WeatherType getWeather(const PointInt& tile) const;
        model::TerrainType getTerrain(const PointInt& tile) const;

        double getMaxVisionRange(model::VehicleType type) const;
        double getVisionFactor(model::WeatherType weather) const;
        double getVisionFactor(model::TerrainType terrain) const;
        double getMobilityFactor(model::WeatherType weather) const;
        double getMobilityFactor(model::TerrainType terrain) const;

        Constants(double helicopterRadius, const PointInt& tileSize, 
                  GroundVisibility&& groundVisibility, GroundMobility&& groundMobility,
                  AirVisibility&& airVisibility, AirMobility&& airMobility, 
                  UnitVisionRadius&& unitVision,
                  const TerrainCells& terrain, const WeatherCells& weather)
            : m_helicoprerRadius(helicopterRadius), m_tileSize(tileSize)
            , m_groundVisibility(groundVisibility), m_groundMobility(groundMobility)
            , m_airVisibility(airVisibility), m_airMobility(airMobility)
            , m_unitVision(unitVision), m_terrain(terrain), m_weather(weather)
        {}
    };

    void updateSelection();
    void initConstants();
    void updateNuclearGuide();
    void updateVehicles();
	void updateEnemyStats();
    void updateFacilities();

public:

    // TODO: move somewhere:
    enum
    {
        GROUP_ARRV_EVEN = 1,
    };

    State() : m_world(nullptr), m_game(nullptr), m_move(nullptr), m_player(nullptr)
            , m_isMoveCommitted(false), m_nuclearGuideGroup(nullptr) 
    {}

    Constants& constants() { return *m_constants; }

    bool                  hasVehicleById(Id id) const { return m_vehicles.find(id) != m_vehicles.end(); }
    const model::Vehicle& vehicleById(Id id)    const { return *m_vehicles.find(id)->second; }
    model::Vehicle&       vehicleById(Id id)          { return *m_vehicles.find(id)->second; }

    const VehicleByID&    getAllVehicles() const      { return m_vehicles; }

    const model::World*  world()    const { return m_world; };
    const model::Player* player()   const { return m_player; };
    const model::Game*   game()     const { return m_game; };

    const EnemyStrategyStats& enemyStrategy() const              { return m_enemyStats; }
    bool enemyDoesNotRush() const;

	const GroupByType&  teammates() const                        { return m_teammates; }
	const VehicleGroup& teammates(model::VehicleType type) const { return m_teammates.find(type)->second; }
	const VehicleGroup& alliens(model::VehicleType type)   const { return m_alliens.find(type)->second; }

    const VehicleGroup* nuclearGuideGroup() const                { return m_nuclearGuideGroup; }
    Point nuclearMissileTarget() const                           { return m_nuclearGuideGroup ? Point(m_player->getNextNuclearStrikeX(), m_player->getNextNuclearStrikeY()) : Point(); }
    VehiclePtr nuclearGuideUnit() const;

    bool areFacilitiesEnabled() const                            { return !m_facilities.empty(); }
    const FacilityById&    facilities() const                    { return m_facilities; }
    const model::Facility* facility(Id id) const                 { auto found = m_facilities.find(id); return found != m_facilities.end() ? &found->second : nullptr; }

    bool isMoveCommitted() const                                 { return m_isMoveCommitted; }
    bool hasActionPoint() const                                  { return player()->getRemainingActionCooldownTicks() == 0; }
    bool isCorrectPosition(const Point& p) const                 { return p.m_x >= 0 && p.m_y >= 0 && p.m_x <= m_game->getWorldWidth() && p.m_y <= m_game->getWorldHeight();}
    bool isCorrectPosition(const Rect& r) const                  { return isCorrectPosition(r.m_topLeft) && isCorrectPosition(r.m_bottomRight); }

    double getUnitVisionRange(const model::Vehicle& v) const     { return getUnitVisionRangeAt(v, v); }
    double getUnitVisionRangeAt(const model::Vehicle& v, const Point& pos) const;
    double getUnitSpeedAt(const model::Vehicle& v, const Point& pos) const;

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

