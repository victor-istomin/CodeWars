#include "GoalMixTanksAndHealers.h"
#include "State.h"
#include "VehicleGroup.h"

#include <map>
#include <array>

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
    PosByType actualPositions;
    PosByType desiredPositions;

    getGroundUnitOrder(actualPositions, desiredPositions);

    MovePlan movesByType;
    for (VehicleType type : s_groundUnits)
        movesByType[type] = getMoves(type, actualPositions[type], desiredPositions[type]);


//    bool isNeedIfvShift = rows[VehicleType::ARRV] == rows[VehicleType::IFV] && rows[VehicleType::ARRV] == rows[VehicleType::TANK];

    auto hasActionPoint = [this]() { return state().hasActionPoint(); };

    static const double GAP_FACTOR = 1.2;

    Point shiftPoint = ifvGroup().m_center + (Point(0, ifvGroup().m_rect.height()) * GAP_FACTOR);

//     if (isNeedIfvShift)
//     {
//         auto isMoveCompleted = [this]() 
//         {
//             const VehicleGroup& ifv = ifvGroup(); 
//             return ifv.getPredictedCenter() == ifv.m_center; 
//         };
// 
//         pushNextStep(NeverAbort(), hasActionPoint, [this]() { return shiftIfv(); }, "mix: shift IFV");
//         pushNextStep(NeverAbort(), isMoveCompleted, []
// 
//     }
}

MixTanksAndHealers::~MixTanksAndHealers()
{
}

void MixTanksAndHealers::getGroundUnitOrder(PosByType& actualPositions, PosByType& desiredPositions)
{

    for (VehicleType type : s_groundUnits)
    {
        const VehicleGroup& group = state().teammates(type);
        actualPositions[type] = GridPos{ static_cast<int>(group.m_rect.m_topLeft.m_x / group.m_rect.height()),
                                         static_cast<int>(group.m_rect.m_topLeft.m_y / group.m_rect.width()) };
    }

    // get mean row
    std::array<int, 3> rowsArray = { actualPositions[VehicleType::ARRV].m_y, actualPositions[VehicleType::IFV].m_y, actualPositions[VehicleType::TANK].m_y };
    std::sort(rowsArray.begin(), rowsArray.end());
    int medianRow = rowsArray[1];

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

MixTanksAndHealers::Destinations MixTanksAndHealers::getMoves(VehicleType groupType, const Point& actualCenter, const GridPos& actual, const GridPos& destination)
{
    if (actual == destination)
        return Destinations();

    const VehicleGroup& initialGroup = state().teammates(groupType);
    VehicleGroupGhost ghost = VehicleGroupGhost(initialGroup, actualCenter - initialGroup.m_center);

    std::vector<const VehicleGroup*> obstacles;

    for (VehicleType type : s_groundUnits)
        if (type != groupType)
            obstacles.push_back(&state().teammates(type));

    const int dx = destination.m_x - actual.m_x;
    const int dy = destination.m_y - actual.m_y;
    Point to = ghost.m_center + (Point(dx, dy) * ghost.m_rect.width());

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
        const Point   firstStageResult = ghost.m_center + (Point(dx, 0) * ghost.m_rect.width());

        Destinations xPart = getMoves(groupType, ghost.m_center, actual, firstStage);
        Destinations yPart = getMoves(groupType, firstStageResult, firstStage, destination);

        path.insert(path.end(), xPart.begin(), xPart.end());
        path.insert(path.end(), xPart.begin(), xPart.end());

        // try alt path: by Y and then by  X
        const GridPos altStage       = GridPos(actual.m_x, actual.m_y + dy);
        const Point   altStageResult = ghost.m_center + (Point(0, dy) * ghost.m_rect.width());

        Destinations xPartAlt = getMoves(groupType, ghost.m_center, actual, altStage);
        Destinations yPartAlt = getMoves(groupType, altStageResult, altStage, destination);

        Destinations altPath;
        altPath.insert(altPath.end(), xPart.begin(), xPart.end());
        altPath.insert(altPath.end(), xPart.begin(), xPart.end());

        // choose shorter alternative
        double primaryLength = getPathLength(ghost.m_center, path);
        double altLength     = getPathLength(ghost.m_center, altPath);

        if (altLength < primaryLength)
            std::swap(altPath, path);

        assert(!path.empty());    // todo: deffer recalculation if no path?
        return path;
    }

    if (dy != 0)
    {
        // 
    }


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

