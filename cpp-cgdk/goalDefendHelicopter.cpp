#include <algorithm>

#define PI 3.14159265358979323846
#define _USE_MATH_DEFINES
#include <cmath>

#include "goalDefendHelicopter.h"
#include "state.h"

#include "model/ActionType.h"
#include "model/VehicleType.h"
#include "model/Move.h"
#include "model/Game.h"

using namespace goals;
using namespace model;


const VehicleGroup& DefendHelicopters::ifvGroup()
{
    return state().teammates(model::VEHICLE_IFV);
}

const VehicleGroup& DefendHelicopters::helicopterGroup()
{
    return state().teammates(model::VEHICLE_HELICOPTER);
}

const VehicleGroup& DefendHelicopters::fighterGroup()
{
    return state().teammates(model::VEHICLE_FIGHTER);
}

const VehicleGroup& DefendHelicopters::tankGroup()
{
    return state().teammates(model::VEHICLE_TANK);
}

bool DefendHelicopters::isPathFree(const VehicleGroup& group, const Point& to, const VehicleGroup& obstacle, double iterationSize)
{
    const Point& groupCenter    = group.m_center;
    const Point& obstacleCenter = obstacle.m_center;

    // TODO: get angle between closest rect corners, not between centers
    double angleBetween = std::abs(Vec2d::angleBetween((to - groupCenter), (obstacleCenter - groupCenter)));

    if (groupCenter.getDistanceTo(obstacleCenter) > groupCenter.getDistanceTo(to) || angleBetween > (PI / 2))
    {
        return true;   // mid-air collision is unlikely
    }

    return canMoveRectTo(groupCenter, to, group.m_rect, obstacle.m_rect, iterationSize);
}

DefendHelicopters::DefendHelicopters(State& state)
    : Goal(state)
    , m_helicopterIteration(std::min(state.constants().m_helicoprerRadius, state.game()->getHelicopterSpeed()) / 2)
{
    auto abortCheckFn     = [this]() { return abortCheck(); };
    auto hasActionPointFn = [this]() { return hasActionPoint(); };

    auto isPathToIfvFree  = [this]() { return isPathFree(helicopterGroup(), ifvGroup().m_center, fighterGroup(), m_helicopterIteration); };

    auto shiftAircraft    = [isPathToIfvFree, abortCheckFn, hasActionPointFn, this]()
    {
        if (isPathToIfvFree())
            return true;  // nothing to move

        const VehicleGroup& fighters    = fighterGroup();
        const VehicleGroup& helicopters = helicopterGroup();

        const Point helicoptersCenter = helicopters.m_center;
        const Point ifvCenter         = ifvGroup().m_center;
        const Point fighterCenter     = fighters.m_center;

        // select and move fighters in order to avoid mid-air collision

        this->state().setSelectAction(fighters.m_rect, VEHICLE_FIGHTER);

        const double near = 1.2;
        const double far  = 2.4;

        Point solutions[] = { tankGroup().m_center,                            // it's fine idea to defend tanks

                              fighterCenter + (Point(fighters.m_rect.width(), 0)  * near),       // right
                              fighterCenter + (Point(0, fighters.m_rect.height()) * near),       // down
                              fighterCenter - (Point(fighters.m_rect.width(), 0)  * near),       // left
                              fighterCenter - (Point(0, fighters.m_rect.height()) * near),       // up

                              fighterCenter + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * near),        // right down
                              fighterCenter - (Point(fighters.m_rect.width(), fighters.m_rect.height()) * near),        // left up

                              fighterCenter - (Point(fighters.m_rect.width(), 0)  * far),        // far left
                              fighterCenter - (Point(0, fighters.m_rect.height()) * far),        // far up
                              fighterCenter + (Point(fighters.m_rect.width(), 0)  * far),        // far right
                              fighterCenter + (Point(0, fighters.m_rect.height()) * far),        // far down

                              fighterCenter + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far),         // far right down
                              fighterCenter - (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far),         // far left up

                              fighterCenter + (Point(fighters.m_rect.width(), fighters.m_rect.height()) * far * far) }; // very far down and right

        auto solutionIt = std::find_if(std::begin(solutions), std::end(solutions),
            [&helicoptersCenter, &ifvCenter, &fighterCenter, &fighters, &helicopters, this](const Point& solution)
        {
            double fightersSize = std::max(fighters.m_rect.width(), fighters.m_rect.height());

            Vec2d solutionPath = Vec2d::fromPoint(solution - fighterCenter);
            Rect  proposedRect = fighters.m_rect + solutionPath;

            if (proposedRect.m_topLeft.m_x < 0 || proposedRect.m_topLeft.m_y < 0
                || solution.getDistanceTo(ifvCenter) < fightersSize    // don't move over IFV
                || solution.getDistanceTo(helicoptersCenter) < fightersSize)  // ... or helicopters
            {
                return false;
            }

            Vec2d fighter2helics   = Vec2d::fromPoint(helicoptersCenter - fighterCenter);
            Vec2d fighter2solution = Vec2d::fromPoint(solution - fighterCenter);

            Vec2d helics2fighter   = Vec2d::fromPoint(fighterCenter - helicoptersCenter);
            Vec2d helics2solution  = Vec2d::fromPoint(solution - helicoptersCenter);

            return canMoveRectTo(helicoptersCenter, ifvCenter, helicopters.m_rect, fighters.m_rect + solutionPath, m_helicopterIteration)
                && canMoveRectTo(fighterCenter, solution, fighters.m_rect, helicopters.m_rect, m_helicopterIteration);
        });

        const Point solution    = solutionIt != std::end(solutions) ? *solutionIt : *std::rbegin(solutions);
        const Vec2d solutonPath = Vec2d::fromPoint(solution - fighterCenter);

        this->pushNextStep(abortCheckFn, hasActionPointFn, [solutonPath, this]() { this->state().setMoveAction(solutonPath); return true; }, "move fighters");

        return true;
    };

    auto selectHelicopters = [this]() { this->state().setSelectAction(helicopterGroup().m_rect, VEHICLE_HELICOPTER); return true; };

    auto moveToJoinPoint = [this]()
    {
        const Point joinPoint  = ifvGroup().m_center;
        const Point selfCenter = helicopterGroup().m_center;

        double distanceTo = selfCenter.getDistanceTo(joinPoint);
        double eta        = distanceTo / this->state().game()->getHelicopterSpeed();   // TODO : use correct prediction

        this->state().setMoveAction(joinPoint - selfCenter);
        return true;
    };

    auto canMove = [this, isPathToIfvFree]() { return hasActionPoint() && isPathToIfvFree(); };

    pushBackStep(abortCheckFn, hasActionPointFn, shiftAircraft,     "ensure no aircraft collision");
    pushBackStep(abortCheckFn, canMove,          selectHelicopters, "select helicopters");
    pushBackStep(abortCheckFn, hasActionPointFn, moveToJoinPoint,   "move helicopters to IFV");
}

bool DefendHelicopters::canMoveRectTo(const Point& from, const Point& to, const Rect& fromRect, const Rect& obstacleRect, double iterationSize)
{
    // TODO - it's possible to perform more careful check
    Vec2d direction = Vec2d::fromPoint(to - from).truncate(iterationSize);
    int stepsTotal = static_cast<int>(std::ceil(from.getDistanceTo(to) / iterationSize));

    bool isPathFree = true;

    for (int i = 0; i < stepsTotal && isPathFree; ++i)
    {
        Rect destination = fromRect + direction * i * iterationSize;
        isPathFree = !destination.overlaps(obstacleRect);
    }

    return isPathFree;
}


