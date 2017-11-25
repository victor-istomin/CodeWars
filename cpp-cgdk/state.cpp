#include "state.h"

void State::update(const model::World& world, const model::Player& me, const model::Game& game, model::Move& move)
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

        const int tileSize = static_cast<int>(game.getWorldWidth()) / world.getWeatherByCellXY().size();

		double helicopterRadius = itHelicopter != m_world->getNewVehicles().end() ? itHelicopter->getRadius() : 2;
		m_constants = std::make_unique<Constants>(helicopterRadius, Constants::PointInt(tileSize, tileSize),
            Constants::GroundVisibility {
                { model::TerrainType::FOREST, game.getForestTerrainVisionFactor() },
                { model::TerrainType::PLAIN,  game.getPlainTerrainVisionFactor() },
                { model::TerrainType::SWAMP,  game.getSwampTerrainVisionFactor() }
            },
            Constants::AirVisibility {
                { model::WeatherType::CLEAR, game.getClearWeatherVisionFactor() },
                { model::WeatherType::CLOUD, game.getCloudWeatherVisionFactor() },
                { model::WeatherType::RAIN,  game.getRainWeatherVisionFactor() }
            },
            Constants::UnitVisionRadius {
                { model::VehicleType::ARRV,       game.getArrvVisionRange()},
                { model::VehicleType::FIGHTER,    game.getFighterVisionRange()},
                { model::VehicleType::HELICOPTER, game.getHelicopterVisionRange()},
                { model::VehicleType::IFV,        game.getIfvVisionRange()},
                { model::VehicleType::TANK,       game.getTankVisionRange()}
            },
            world.getTerrainByCellXY(), world.getWeatherByCellXY());
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

	IdList selection;
	selection.reserve(m_vehicles.size());

	for (const auto& idPointnerPair : m_vehicles)
	{
		const VehiclePtr& vehicle = idPointnerPair.second;
		if (vehicle->getPlayerId() == me.getId() && vehicle->isSelected())
			selection.push_back(vehicle->getId());
	}

	std::sort(selection.begin(), selection.end());
	m_selection = std::move(selection);

	m_nuclearGuide = nullptr;
	if (me.getNextNuclearStrikeTickIndex() != -1)
	{
		Id guideId = me.getNextNuclearStrikeVehicleId();

		VehiclePtr guideUnit = m_vehicles[guideId];

		if (guideUnit)
			m_nuclearGuide = &teammates(guideUnit->getType());
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

void State::setAddToSelectionAction(const Rect& rect, model::VehicleType vehicleType /*= model::VehicleType::_UNKNOWN_*/)
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
