#pragma once
#include <memory>
#include <vector>
#include "geometry.h"

#include "model/Vehicle.h"

typedef std::shared_ptr<model::Vehicle> VehiclePtr;
typedef std::weak_ptr<model::Vehicle>   VehicleCache;

struct VehicleGroup
{
    std::vector<VehicleCache> m_units;
    Point  m_center;
    Rect   m_rect;
    double m_healthSum;
    Point  m_plannedDestination;

    // TODO refactor: add getters/setters
    void  setPlannedDestination(const Point& dest = Point()) { m_plannedDestination = dest; }
    bool  hasPlannedDestination() const                      { return m_plannedDestination != Point(0,0); }
    Point getPredictedCenter() const                         { return hasPlannedDestination() ? m_plannedDestination : m_center; }

    void add(const VehiclePtr& vehicle)     { m_units.emplace_back(vehicle); }

    void update();
    
    bool isPathFree(const Point& to, const VehicleGroup& obstacle, double iterationSize) const;
    static bool canMoveRectTo(const Point& from, const Point& to, const Rect& fromRect, const Rect& obstacleRect, double iterationSize);

    bool mayIntersect(const VehicleGroup& other, const Vec2d& thisSpeed, const Vec2d& otherSpeed) const;

    VehicleGroup getGhost(const Vec2d& displacement) const
    {
        // TODO - safe code using inherited VehicleGroupGhost!!!
        return VehicleGroup{ std::vector<VehicleCache>(), m_center + displacement, m_rect + displacement };
    }
};

