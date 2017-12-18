#include "state.h"
#include <cmath>
#include <numeric>

using namespace model;   // TODO: cleanup

const float State::EnemyStrategyStats::MAX_SCORE         = 1.0;
const float State::EnemyStrategyStats::POSITIVE_SCORE    = 0.5 * MAX_SCORE;
const float State::EnemyStrategyStats::INCREMENT_DIVIDER = 2.0;
const float State::EnemyStrategyStats::INCREMENT         = MAX_SCORE / INCREMENT_DIVIDER;   // so, limit will be MAX_SCORE


void State::update(const model::World& world, const model::Player& me, const model::Game& game, model::Move& move)
{
    assert(me.isMe());
    m_world  = &world;
    m_player = &me;
    m_game   = &game;
    m_move   = &move;
    m_enemy  = & (*std::find_if(world.getPlayers().begin(), world.getPlayers().end(), [](const Player& p) { return !p.isMe(); }));

    initConstants();
    initState();

    m_isMoveCommitted = false;

    updateVehicles();

    updateGroups();

    updateSelection();

    updateNuclearGuide();

    updateFacilities();

    updateEnemyStats();
}

void State::updateGroups()
{
    for (auto& group : m_teammates)
        group.second.update();
    updateGroupsRect(m_teammates, m_teammatesRect);

    for (auto& group : m_alliens)
        group.second.update();
    updateGroupsRect(m_alliens, m_alliensRect);

    for (auto& group : m_newTeammates)
        group.second.update();
}

void State::updateVehicles()
{
    for (const model::Vehicle& v : m_world->getNewVehicles())
    {
        auto newVehicle = std::make_shared<model::Vehicle>(v);
        m_vehicles[v.getId()] = newVehicle;

		bool isTeammate = v.getPlayerId() == m_player->getId();
		bool isProduced = isTeammate && m_world->getTickIndex() > 1;

        GroupHandle initialGroup = GroupHandle::initial(v);

		if (isProduced)
		{
            auto existingGroup = m_teammates[initialGroup];

            if (existingGroup.m_rect.contains(v))
                existingGroup.add(newVehicle);
            else
                m_newTeammates[GroupHandle::artificial(v)].add(newVehicle);   // vehicle produced by factory outside its group, put it the the separate list and wait for merge
		}
		else if (isTeammate)
		{
			m_teammates[initialGroup].add(newVehicle);
		}
		else
		{
			m_alliens[initialGroup].add(newVehicle);
		}
    }

    for (const model::VehicleUpdate& update : m_world->getVehicleUpdates())
    {
        if (update.getDurability() != 0)
            vehicleById(update.getId()) = model::Vehicle(vehicleById(update.getId()), update);
        else
            m_vehicles.erase(update.getId());
    }
}

void State::updateEnemyStats()
{
	const Point myBase = { 100, 100 };

	static const VehicleType s_groundUnits[] = { VehicleType::ARRV, VehicleType::TANK, VehicleType::IFV };
	static const VehicleType s_allUnits[]    = { VehicleType::ARRV, VehicleType::TANK, VehicleType::IFV , VehicleType::FIGHTER, VehicleType::HELICOPTER };

	static const int MAX_START_PHASE_TICKS = 3000; // QuickStart guy arrives at ~500 tick, so this looks enough for start
	auto notExistent = [this](VehicleType type) { return m_alliens.find(GroupHandle::initial(type)) == m_alliens.end(); };

    bool isStartPhase = world()->getTickIndex() < MAX_START_PHASE_TICKS
        && std::find_if(std::begin(s_allUnits), std::end(s_allUnits), notExistent) == std::end(s_allUnits);

	if (isStartPhase)
	{
        // --- decay old scores

        m_enemyStats.m_startedWithAirRush  /= EnemyStrategyStats::INCREMENT_DIVIDER;
        m_enemyStats.m_startedWithSlowHeap /= EnemyStrategyStats::INCREMENT_DIVIDER;

		// --- detect whether aircraft rushes me ahead of other enemy troops 

		const VehicleGroup& enemyFighters    = alliens(VehicleType::FIGHTER);
		const VehicleGroup& enemyHelicopters = alliens(VehicleType::HELICOPTER);
		const Point&        fightersCenter   = enemyFighters.m_center;

		// TODO: detect nuking by 1-2 aircrafts

		const double nearestAirEnemydistanceSq = std::min(myBase.getSquareDistance(fightersCenter), myBase.getSquareDistance(enemyHelicopters.m_center));

		auto isAheadOfAircraft = [this, &myBase, &nearestAirEnemydistanceSq](VehicleType type) { return myBase.getSquareDistance(alliens(type).m_center) < nearestAirEnemydistanceSq; };
		bool isAircraftAhead   = std::find_if(std::begin(s_groundUnits), std::end(s_groundUnits), isAheadOfAircraft) == std::end(s_groundUnits);

		if (isAircraftAhead)
		{
			double aircraftGap = std::numeric_limits<double>::max();
			for (VehicleType groundType : s_groundUnits)
				aircraftGap = std::min(aircraftGap, fightersCenter.getDistanceTo(alliens(groundType).m_center));

			const double s_stillTogetherDistance = std::hypot(enemyFighters.m_rect.height(), enemyFighters.m_rect.width());
			isAircraftAhead = aircraftGap > s_stillTogetherDistance;
		}

		if (isAircraftAhead)
		{
			m_enemyStats.m_startedWithAirRush += EnemyStrategyStats::INCREMENT;   // limit is MAX_SCORE
		}

		// --- detect "slow heap" strategy
        static const size_t k_unitTypes = std::extent<decltype(s_allUnits)>::value;

        size_t intersections = 0;
        for(size_t i = 0; i < k_unitTypes; ++i)
        { 
            const Rect& groupRect = alliens(s_allUnits[i]).m_rect;
            for (size_t j = i + 1; j < k_unitTypes; ++j)
            {
                const Rect& otherRect = alliens(s_allUnits[j]).m_rect;
                if (groupRect.overlaps(otherRect))
                    ++intersections;
            };            
        }

        static const size_t k_minIntersectionsInHeap = k_unitTypes - 1;  // all groups intersects with one neighbor
        if (intersections >= k_minIntersectionsInHeap)
        {
            m_enemyStats.m_startedWithSlowHeap += EnemyStrategyStats::INCREMENT;
        }
	}
}

void State::updateFacilities()
{
    for (const model::Facility& f : world()->getFacilities())
        m_facilities[f.getId()] = f;
}

void State::updateNuclearGuide()
{
    m_nuclearGuideGroup = nullptr;
    if (m_player->getNextNuclearStrikeTickIndex() != -1)
    {
        Id guideId = m_player->getNextNuclearStrikeVehicleId();

        VehiclePtr guideUnit = m_vehicles[guideId];

        if (guideUnit)
            m_nuclearGuideGroup = &teammates(guideUnit->getType());
    }
}

void State::updateSelection()
{
    IdList selection;
    selection.reserve(m_vehicles.size());

    for (const auto& idPointnerPair : m_vehicles)
    {
        const VehiclePtr& vehicle = idPointnerPair.second;
        if (vehicle->getPlayerId() == m_player->getId() && vehicle->isSelected())
            selection.push_back(vehicle->getId());
    }

    std::sort(selection.begin(), selection.end());
    m_selection = std::move(selection);
}

void State::initConstants()
{
    if (m_constants)
        return;

    auto itHelicopter = std::find_if(m_world->getNewVehicles().begin(), m_world->getNewVehicles().end(),
        [](const model::Vehicle& v) { return v.getType() == model::VehicleType::HELICOPTER; });

    const int tileSize = static_cast<int>(m_game->getWorldWidth()) / m_world->getWeatherByCellXY().size();

    double helicopterRadius = itHelicopter != m_world->getNewVehicles().end() ? itHelicopter->getRadius() : 2;

    m_constants = std::make_unique<Constants>
    (
        helicopterRadius, Constants::PointInt(tileSize, tileSize),
        Constants::GroundVisibility{
            { model::TerrainType::FOREST, m_game->getForestTerrainVisionFactor() },
            { model::TerrainType::PLAIN,  m_game->getPlainTerrainVisionFactor() },
            { model::TerrainType::SWAMP,  m_game->getSwampTerrainVisionFactor() }
        },
        Constants::GroundMobility{
            { model::TerrainType::FOREST, m_game->getForestTerrainSpeedFactor() },
            { model::TerrainType::PLAIN,  m_game->getPlainTerrainSpeedFactor() },
            { model::TerrainType::SWAMP,  m_game->getSwampTerrainSpeedFactor() }
        },
        Constants::AirVisibility{
            { model::WeatherType::CLEAR, m_game->getClearWeatherVisionFactor() },
            { model::WeatherType::CLOUD, m_game->getCloudWeatherVisionFactor() },
            { model::WeatherType::RAIN,  m_game->getRainWeatherVisionFactor() }
        },
        Constants::AirMobility{
            { model::WeatherType::CLEAR, m_game->getClearWeatherSpeedFactor() },
            { model::WeatherType::CLOUD, m_game->getCloudWeatherSpeedFactor() },
            { model::WeatherType::RAIN,  m_game->getRainWeatherSpeedFactor() }
        },
        Constants::UnitVisionRadius{
            { model::VehicleType::ARRV,       m_game->getArrvVisionRange()},
            { model::VehicleType::FIGHTER,    m_game->getFighterVisionRange()},
            { model::VehicleType::HELICOPTER, m_game->getHelicopterVisionRange()},
            { model::VehicleType::IFV,        m_game->getIfvVisionRange()},
            { model::VehicleType::TANK,       m_game->getTankVisionRange()}
        },
        m_world->getTerrainByCellXY(), m_world->getWeatherByCellXY()
    );
}

void State::initState()
{
    if (m_world->getTickIndex() == 0)
    {
        model::VehicleType allTypes[] = { model::VehicleType::ARRV, model::VehicleType::FIGHTER, model::VehicleType::HELICOPTER, model::VehicleType::IFV, model::VehicleType::TANK };

        for (auto type : allTypes)
            m_alliens.emplace(std::make_pair(GroupHandle::initial(type), VehicleGroup()));
    }
}

void State::updateGroupsRect(const GroupByType& groupsMap, Rect& rect)
{
	static const Rect k_nullRect = Rect();

	rect = k_nullRect;

	for (const auto& idGroupPair : groupsMap)
	{
		const Rect& groupRect = idGroupPair.second.m_rect;

		if (rect == k_nullRect)
		{
			rect = groupRect;
		}
		else
		{
			rect.ensureContains(groupRect);
		}
	}
}

void State::setSelectAction(const Rect& rect, model::VehicleType vehicleType /*= model::VehicleType::_UNKNOWN_*/)
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

void State::setSelectAction(const VehicleGroup& group)
{
	bool isSomethingToSelect = !group.m_units.empty();

	if (!m_selection.empty())
	{
		IdList desiredSelection;
		desiredSelection.reserve(group.m_units.size());

		for (const VehicleCache& unitCache : group.m_units)
			desiredSelection.push_back(unitCache.lock()->getId());

		std::sort(desiredSelection.begin(), desiredSelection.end());

		if (desiredSelection == m_selection)
			isSomethingToSelect = false;  // already selected
	}

	if (isSomethingToSelect)
		setSelectAction(group.m_rect, group.m_units.front().lock()->getType());
}

void State::setSelectAction(int groupId)
{
    m_move->setAction(model::ActionType::CLEAR_AND_SELECT);
    m_move->setGroup(groupId);

    m_isMoveCommitted = true;
}

void State::setAddSelectionAction(const Rect& rect, model::VehicleType vehicleType /*= model::VehicleType::_UNKNOWN_*/)
{
    m_move->setAction(model::ActionType::ADD_TO_SELECTION);

    if (vehicleType != model::VehicleType::_UNKNOWN_)
        m_move->setVehicleType(vehicleType);

    m_move->setTop(rect.m_topLeft.m_y);
    m_move->setLeft(rect.m_topLeft.m_x);
    m_move->setBottom(rect.m_bottomRight.m_y);
    m_move->setRight(rect.m_bottomRight.m_x);

    m_isMoveCommitted = true;
}

void State::setDeselectAction(const Rect& rect, model::VehicleType vehicleType /*= model::VehicleType::_UNKNOWN_*/)
{
	m_move->setAction(model::ActionType::DESELECT);

	if (vehicleType != model::VehicleType::_UNKNOWN_)
		m_move->setVehicleType(vehicleType);

	m_move->setTop(rect.m_topLeft.m_y);
	m_move->setLeft(rect.m_topLeft.m_x);
	m_move->setBottom(rect.m_bottomRight.m_y);
	m_move->setRight(rect.m_bottomRight.m_x);

	m_isMoveCommitted = true;
}

void State::setAssignGroupAction(int groupNumber)
{
    m_move->setAction(model::ActionType::ASSIGN);
    m_move->setGroup(groupNumber);

    m_isMoveCommitted = true;
}

void State::setMoveAction(const Vec2d& vector, double maxSpeed /*= -1*/)
{
	m_move->setAction(model::ActionType::MOVE);
	m_move->setX(vector.m_x);
	m_move->setY(vector.m_y);

    if (maxSpeed > 0)
        m_move->setMaxSpeed(maxSpeed);

	m_isMoveCommitted = true;
}

void State::setNukeAction(const Point& point, const model::Vehicle& guide)
{
	m_move->setAction(model::ActionType::TACTICAL_NUCLEAR_STRIKE);
	m_move->setX(point.m_x);
	m_move->setY(point.m_y);
	m_move->setVehicleId(guide.getId());

	m_isMoveCommitted = true;
}

void State::setScaleAction(double factor, const Point& center)
{
	m_move->setAction(model::ActionType::SCALE);
	m_move->setX(center.m_x);
	m_move->setY(center.m_y);
	m_move->setFactor(factor);

	m_isMoveCommitted = true;
}

void State::setProduceAction(State::Id facilityId, model::VehicleType type /*= model::VehicleType::_UNKNOWN_*/)
{
	m_move->setAction(ActionType::SETUP_VEHICLE_PRODUCTION);
	m_move->setFacilityId(facilityId);
	
	if (type != VehicleType::_UNKNOWN_)
		m_move->setVehicleType(type);

	m_isMoveCommitted = true;
}

State::Constants::PointInt State::Constants::getTileIndex(const Point& p) const
{
    return PointInt(static_cast<int>(p.m_x) / m_tileSize.m_x, static_cast<int>(p.m_y) / m_tileSize.m_y);
}

model::WeatherType State::Constants::getWeather(const PointInt& tile) const
{
    bool isValidRange = tile.m_x < (int)m_weather.size() && tile.m_y < (int)m_weather.front().size();
    assert(isValidRange);
    if (!isValidRange)
        return WeatherType::CLEAR;

    return m_weather[tile.m_x][tile.m_y];
}

model::TerrainType State::Constants::getTerrain(const PointInt& tile) const
{
    bool isValidRange = tile.m_x < (int)m_terrain.size() && tile.m_y < (int)m_terrain.front().size();
    assert(isValidRange);
    if (!isValidRange)
        return TerrainType::PLAIN;

    return m_terrain[tile.m_x][tile.m_y];
}

double State::Constants::getMaxVisionRange(model::VehicleType type) const
{
    return m_unitVision.find(type)->second;
}

double State::Constants::getVisionFactor(model::WeatherType weather) const
{
    auto found = m_airVisibility.find(weather);
    assert(found != m_airVisibility.end());
    return found != m_airVisibility.end() ? found->second : 1.0;
}

double State::Constants::getVisionFactor(model::TerrainType terrain) const
{
    auto found = m_groundVisibility.find(terrain);
    assert(found != m_groundVisibility.end());
    return found != m_groundVisibility.end() ? found->second : 1.0;
}

double State::Constants::getMobilityFactor(model::WeatherType weather) const
{
    auto found = m_airMobility.find(weather);
    assert(found != m_airMobility.end());
    return found != m_airMobility.end() ? found->second : 1.0;
}

double State::Constants::getMobilityFactor(model::TerrainType terrain) const
{
    auto found = m_groundMobility.find(terrain);
    assert(found != m_groundMobility.end());
    return found != m_groundMobility.end() ? found->second : 1.0;
}

bool State::enemyDoesNotRush() const
{
    const int tickIndex = world()->getTickIndex();
    return tickIndex > Constants::DEFEND_DESICION_TICK && !enemyStrategy().isAirRush();
}

bool State::enemyDoesNotHeap() const
{
    const int tickIndex = world()->getTickIndex();
    return tickIndex > Constants::DEFEND_DESICION_TICK && !enemyStrategy().isSlowHeap();
}

size_t State::newTeammatesCount() const
{
    return std::accumulate(m_newTeammates.begin(), m_newTeammates.end(), 0, 
        [](int old, const auto& idGroupPair) { return old += idGroupPair.second.m_units.size(); });
}

State::GroupByType State::popNewUnits()
{
    GroupByType newPortion;
    std::swap(newPortion, m_newTeammates);
    return newPortion;
}

void State::mergeNewUnits(const std::pair<GroupHandle, VehicleGroup>& newUnits)
{
    m_teammates.insert(newUnits);
    updateGroups();
}

VehiclePtr State::nuclearGuideUnit() const
{
    Id guideId = m_player->getNextNuclearStrikeVehicleId();

    auto found = m_vehicles.find(guideId);
    return found != m_vehicles.end() ? found->second : nullptr;
}

double State::getUnitVisionRangeAt(const model::Vehicle& v, const Point& pos) const
{
    Constants::PointInt tile = m_constants->getTileIndex(pos);
    double initial = m_constants->getMaxVisionRange(v.getType());
    double factor = v.isAerial() ? m_constants->getVisionFactor(m_constants->getWeather(tile)) : m_constants->getVisionFactor(m_constants->getTerrain(tile));
    return initial * factor;
}

double State::getUnitSpeedAt(const model::Vehicle& v, const Point& pos) const
{
    Constants::PointInt tile = m_constants->getTileIndex(pos);
    double factor = v.isAerial() ? m_constants->getMobilityFactor(m_constants->getWeather(tile)) : m_constants->getMobilityFactor(m_constants->getTerrain(tile));
    return v.getMaxSpeed() * factor;
}

// check is this enemy group intersects with another enemy group in order to detect massive rush
bool State::isEnemyCoveredByAnother(model::VehicleType groupId, VehicleGroup& mergedGroups) const
{
    mergedGroups = alliens(groupId);

    std::vector<const VehicleGroup*> others;
    others.reserve(m_alliens.size());

    for (const auto& idGroupPair : m_alliens)
        if (idGroupPair.first.vehicleType() != groupId)
            others.push_back(&idGroupPair.second);

    bool isCovered = false;
    for (const VehicleGroup* other : others)
    {
        if (mergedGroups.m_rect.overlaps(other->m_rect))
        {
            // merge
            isCovered = true;
            std::copy(other->m_units.begin(), other->m_units.end(), std::inserter(mergedGroups.m_units, mergedGroups.m_units.end()));
        }
    }

    if (isCovered)
        mergedGroups.update();

    return isCovered;
}

// return closest distance between teammate rect and alliens rect. 0 if rect overlaps.
double State::getDistanceToAlliensRect() const
{
	if (m_teammatesRect.overlaps(m_alliensRect))
		return 0;

	const Point teammatePoints[] = { m_teammatesRect.m_topLeft, m_teammatesRect.bottomLeft(), 
	                                 m_teammatesRect.topRight(), m_teammatesRect.m_bottomRight };

	const Point alliensPoints[] = { m_alliensRect.m_topLeft,  m_alliensRect.bottomLeft(),
	                                m_alliensRect.topRight(), m_alliensRect.m_bottomRight };

	// trivial O(n^2) is just fine when n = 4
	double closestDistance = teammatePoints[0].getDistanceTo(alliensPoints[0]);
	for (const Point& teammateCorner : teammatePoints)
		for (const Point& allienCorner : alliensPoints)
			closestDistance = std::min(closestDistance, teammateCorner.getDistanceTo(allienCorner));

	return closestDistance;
}

bool State::isValidWorldPoint(const Point& p) const
{
    return p.m_x >= 0 && p.m_x < game()->getWorldWidth()
        && p.m_y >= 0 && p.m_y < game()->getWorldHeight();
}

