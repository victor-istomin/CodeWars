#include "GoalMixTanksAndHealers.h"
#include "State.h"
#include "VehicleGroup.h"

#include <map>
#include <array>
#include <limits>

using namespace goals;
using namespace model;

const VehicleType MixTanksAndHealers::s_groundUnits[] = { VehicleType::ARRV, VehicleType::IFV, VehicleType::TANK };

namespace
{
    enum class FormationOrder
    {
        eFT_UNKNOWN = 0,     // not valid
        dFT_IVF_HEALER_TANK,
        eFT_TANK_HEALER_IFV,
    };
}

MixTanksAndHealers::MixTanksAndHealers(State& worldState)
    : Goal(worldState)
    , m_iterationSize( std::min( {worldState.game()->getArrvSpeed(), worldState.game()->getTankSpeed(), worldState.game()->getIfvSpeed()} ) / 2.0 )
{
	initGridPositions();

    PosByType actualPositions;
    PosByType desiredPositions;

    getGroundUnitOrder(actualPositions, desiredPositions);

    MovePlan movesByType;
    for (VehicleType type : s_groundUnits)
        movesByType[type] = getMoves(type, state().teammates(type).m_center, actualPositions[type], desiredPositions[type], true);

	std::list<int> todo;


}

MixTanksAndHealers::~MixTanksAndHealers()
{
}

void MixTanksAndHealers::getGroundUnitOrder(PosByType& actualPositions, PosByType& desiredPositions)
{
    for (VehicleType type : s_groundUnits)
    {
        const VehicleGroup& group = state().teammates(type);
        actualPositions[type] = pointToPos(group.m_center);
    }

    // get mean row
    std::array<int, 3> rowsArray = { actualPositions[VehicleType::ARRV].m_y, actualPositions[VehicleType::IFV].m_y, actualPositions[VehicleType::TANK].m_y };
    std::sort(rowsArray.begin(), rowsArray.end());
	int medianRow = 1;    // TODO - some optimization when we have a lot of rowsArray with 2nd row

    std::list<int> freeColumns = { 0, 1, 2 };

    auto removeColumn = [](std::list<int>& columns, int column)
    {
        auto itFound = std::find(columns.begin(), columns.end(), column);
        assert(itFound != columns.end());
        if (itFound != columns.end())
            columns.erase(itFound);
    };

    for (const std::pair<VehicleType, GridPos>& actualPair : actualPositions)
    {
        if (actualPair.second.m_y == medianRow)
        {
            removeColumn(freeColumns, actualPair.second.m_x);
            desiredPositions[actualPair.first] = actualPair.second;
        }
    }

    // move others to mean row

    auto alreadyHasXFn = [&desiredPositions](int x) { return [&desiredPositions, x](const std::pair<VehicleType, GridPos>& p) { return p.second.m_x == x; }; };

    for (const std::pair<VehicleType, GridPos>& actualPair : actualPositions)
    {
        VehicleType type = actualPair.first;
        if (desiredPositions.find(type) != desiredPositions.end())
            continue;   // already in its place

        const GridPos& actualPos = actualPair.second;

        GridPos newPos = { 0, 0 };
        if (std::find_if(desiredPositions.begin(), desiredPositions.end(), alreadyHasXFn(actualPos.m_x)) == desiredPositions.end())
        {
            // just move up-down, easy move -> high priority
            newPos = { actualPos.m_x, medianRow };
        }
        else
        {
            freeColumns.sort([actualPos](int left, int right) { return std::abs(actualPos.m_x - left) < std::abs(actualPos.m_x - right); });

            assert(!freeColumns.empty());
            newPos = { freeColumns.empty() ? (int)desiredPositions.size() : freeColumns.front(), medianRow };
        }

        removeColumn(freeColumns, newPos.m_x);
        desiredPositions[type] = newPos;
    }
}

MixTanksAndHealers::Destinations MixTanksAndHealers::getMoves(
    VehicleType groupType, const Point& actualCenter, const GridPos& actual, const GridPos& destination, bool allowShifting)
{
    const VehicleGroup& initialGroup = state().teammates(groupType);
    VehicleGroupGhost ghost = VehicleGroupGhost(initialGroup, actualCenter - initialGroup.m_center);

	const int dx = destination.m_x - actual.m_x;
	const int dy = destination.m_y - actual.m_y;
	Point to = posToPoint(destination);

	if (actual == destination)
		return Destinations({ to });

    std::vector<const VehicleGroup*> obstacles;

    for (VehicleType type : s_groundUnits)
        if (type != groupType)
            obstacles.push_back(&state().teammates(type));

    bool isStraightWay = true;
    for (const VehicleGroup* obstacle : obstacles)
        if (isStraightWay && !ghost.isPathFree(to, Obstacle(*obstacle), m_iterationSize))
            isStraightWay = false;

    if (isStraightWay)
        return Destinations({ to });

    // find a path through obstacles

    Destinations path;
    if (dx != 0 && dy != 0)
    {
        // move by X, then move by Y
        const GridPos firstStage       = GridPos(actual.m_x + dx, actual.m_y);
		const Point   firstStageResult = posToPoint(firstStage);

        Destinations xPart = getMoves(groupType, ghost.m_center, actual, firstStage, false);
        Destinations yPart = getMoves(groupType, firstStageResult, firstStage, destination, false);

		if (!xPart.empty() && !yPart.empty())
		{
			path.insert(path.end(), xPart.begin(), xPart.end());
			path.insert(path.end(), xPart.begin(), xPart.end());
		}

        // try alt path: by Y and then by  X
        const GridPos altStage       = GridPos(actual.m_x, actual.m_y + dy);
		const Point   altStageResult = posToPoint(altStage);

        Destinations xPartAlt = getMoves(groupType, ghost.m_center, actual, altStage, false);
        Destinations yPartAlt = getMoves(groupType, altStageResult, altStage, destination, false);

        Destinations altPath;
		if (!xPartAlt.empty() && !yPartAlt.empty())
		{
			altPath.insert(altPath.end(), xPart.begin(), xPart.end());
			altPath.insert(altPath.end(), xPart.begin(), xPart.end());
		}

        // choose shorter alternative
        double primaryLength = path.empty()    ? std::numeric_limits<double>::max() : getPathLength(ghost.m_center, path);
        double altLength     = altPath.empty() ? std::numeric_limits<double>::max() : getPathLength(ghost.m_center, altPath);

        if (altLength < primaryLength)
            std::swap(altPath, path);
    }
	else if (dy != 0 && allowShifting)
    {
		assert(dx == 0);  // this code is for vertical move

		bool shiftLeft = actual.m_x > 0 && (actual.m_y % 2 == 1);    // alternating shift direction in order to avoid repeating collisions
		const int newDx = shiftLeft ? -1 : 1;

		const GridPos firstStage = GridPos(actual.m_x + newDx, actual.m_y);
		const Point   firstStageResult = posToPoint(firstStage);

		Destinations xPart = getMoves(groupType, actualCenter, actual, firstStage, false);
		Destinations yPart = getMoves(groupType, firstStageResult, firstStage, destination, false);

		if (!xPart.empty() && !yPart.empty())
		{
			path.insert(path.end(), xPart.begin(), xPart.end());
			path.insert(path.end(), xPart.begin(), xPart.end());
		}
    }
	else if (dx != 0 && allowShifting)
	{
		assert(dy == 0);  // this code is for horizontal move

		bool shiftUp = actual.m_y > 0 && (actual.m_x % 2 == 1);    // alternating shift direction in order to avoid repeating collisions
		const int newDy = shiftUp ? -1 : 1;

		const GridPos firstStage = GridPos(actual.m_x, actual.m_y + newDy);
		const Point   firstStageResult = posToPoint(firstStage);

		Destinations xPart = getMoves(groupType, actualCenter, actual, firstStage, false);
		Destinations yPart = getMoves(groupType, firstStageResult, firstStage, destination, false);

		if (!xPart.empty() && !yPart.empty())
		{
			path.insert(path.end(), xPart.begin(), xPart.end());
			path.insert(path.end(), xPart.begin(), xPart.end());
		}
	}

	return path;
}

double MixTanksAndHealers::getPathLength(const Point& start, const Destinations& path)
{
    double length = 0;
    Point current = start;
    for (const Point& next : path)
    {
        length += current.getDistanceTo(next);
        current = next;
    }

    return length;
}

Point MixTanksAndHealers::posToPoint(const GridPos& pos)
{
	assert(m_xGridToPos.size() > 1 && "at least 2 columns needed to approximate"); 
	if (m_xGridToPos.find(pos.m_x) == m_xGridToPos.end() && m_xGridToPos.size() > 1)
	{
		// cache not found, approximate
		int    backColumn  = m_xGridToPos.rbegin()->first;
		int    frontColumn = m_xGridToPos.begin()->first;
		double backPos     = m_xGridToPos.rbegin()->second;
		double frontPos    = m_xGridToPos.begin()->second;

		double columnWidth = (backPos- frontPos) / (backColumn - frontColumn);
		double columnPos   = backPos + (pos.m_x - backColumn) * columnWidth;

		m_xGridToPos[pos.m_x] = columnPos;
	}

	assert(m_yGridToPos.size() > 1 && "at least 2 rows needed to approximate");
	if (m_yGridToPos.find(pos.m_y) == m_yGridToPos.end() && m_yGridToPos.size() > 1)
	{
		// cache not found, approximate
		int    backRow  = m_yGridToPos.rbegin()->first;
		int    frontRow = m_yGridToPos.begin()->first;
		double backPos  = m_yGridToPos.rbegin()->second;
		double frontPos = m_yGridToPos.begin()->second;

		double rowWidth = (backPos - frontPos) / (backRow - frontRow);
		double rowPos   = backPos + (pos.m_x - backRow) * rowWidth;

		m_yGridToPos[pos.m_y] = rowPos;
	}

	return Point(m_xGridToPos[pos.m_x], m_yGridToPos[pos.m_y]);
}

void MixTanksAndHealers::initGridPositions()
{
	const State::GroupByType& teammates = state().teammates();
	assert(!teammates.empty());
	if (teammates.empty())
		return;

	m_gridCellSize = Point(teammates.begin()->second.m_rect.width(), teammates.begin()->second.m_rect.height());

	Point topLeftMargin = { std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };

	for (const auto& typeGroupPair : teammates)
	{
		const Point& topLeft = typeGroupPair.second.m_rect.m_topLeft;
		topLeftMargin.m_x = std::min(topLeftMargin.m_x, topLeft.m_x);
		topLeftMargin.m_y = std::min(topLeftMargin.m_y, topLeft.m_y);
	}

	m_topLeftMargin = topLeftMargin;

	for (const auto& typeGroupPair : teammates)
	{
		const VehicleGroup& group = typeGroupPair.second;

		assert(group.m_rect.height() == m_gridCellSize.m_y);
		assert(group.m_rect.width() == m_gridCellSize.m_x);

		GridPos pos = pointToPos(group.m_center);

		m_xGridToPos[pos.m_x] = group.m_center.m_x;
		m_yGridToPos[pos.m_y] = group.m_center.m_y;
	}
}

MixTanksAndHealers::GridPos MixTanksAndHealers::pointToPos(const Point& center)
{
	Point relativeCenter = center - m_topLeftMargin - m_gridCellSize / 2;
	GridPos pos = GridPos { static_cast<int>(relativeCenter.m_x / m_gridCellSize.m_x), static_cast<int>(relativeCenter.m_y / m_gridCellSize.m_y) };
	return pos;
}

