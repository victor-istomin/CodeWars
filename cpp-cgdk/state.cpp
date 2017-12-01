#include "state.h"
#include <cmath>

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

    initConstants();

	m_isMoveCommitted = false;

    updateVehicles();

    updateGroups();

    updateSelection();

    updateNuclearGuide();

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
}

void State::updateVehicles()
{
    for (const model::Vehicle& v : m_world->getNewVehicles())
    {
        auto newVehicle = std::make_shared<model::Vehicle>(v);
        m_vehicles[v.getId()] = newVehicle;

        if (v.getPlayerId() == m_player->getId())
            m_teammates[v.getType()].add(newVehicle);
        else
            m_alliens[v.getType()].add(newVehicle);
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
	auto notExistent = [this](VehicleType type) { return m_alliens.find(type) == m_alliens.end(); };

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
            for (size_t j = i + 1; i < k_unitTypes; ++j)
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

void State::updateNuclearGuide()
{
    m_nuclearGuide = nullptr;
    if (m_player->getNextNuclearStrikeTickIndex() != -1)
    {
        Id guideId = m_player->getNextNuclearStrikeVehicleId();

        VehiclePtr guideUnit = m_vehicles[guideId];

        if (guideUnit)
            m_nuclearGuide = &teammates(guideUnit->getType());
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
        Constants::AirVisibility{
            { model::WeatherType::CLEAR, m_game->getClearWeatherVisionFactor() },
            { model::WeatherType::CLOUD, m_game->getCloudWeatherVisionFactor() },
            { model::WeatherType::RAIN,  m_game->getRainWeatherVisionFactor() }
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

void State::setMoveAction(const Vec2d& vector)
{
	m_move->setAction(model::ActionType::MOVE);
	m_move->setX(vector.m_x);
	m_move->setY(vector.m_y);

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

State::Constants::PointInt State::Constants::getTileIndex(const model::Vehicle& v) const
{
    return PointInt(static_cast<int>(v.getX()) / m_tileSize.m_x, static_cast<int>(v.getY()) / m_tileSize.m_y);
}

model::WeatherType State::Constants::getWeather(const PointInt& tile) const
{
    return m_weather[tile.m_x][tile.m_y];
}

model::TerrainType State::Constants::getTerrain(const PointInt& tile) const
{
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

double State::getUnitVisionRange(const model::Vehicle& v) const
{
    Constants::PointInt tile = m_constants->getTileIndex(v);
    double initial = m_constants->getMaxVisionRange(v.getType());
    double factor = v.isAerial() ? m_constants->getVisionFactor(m_constants->getWeather(tile)) : m_constants->getVisionFactor(m_constants->getTerrain(tile));
    return initial * factor;
}

// check is this enemy group intersects with another enemy group in order to detect massive rush
bool State::isEnemyCoveredByAnother(model::VehicleType groupId, VehicleGroup& mergedGroups) const
{
    mergedGroups = alliens(groupId);

    std::vector<const VehicleGroup*> others;
    others.reserve(m_alliens.size());

    for (const auto& idGroupPair : m_alliens)
        if (idGroupPair.first != groupId)
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

