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
	double m_maxUnitRadius;

    void add(const VehiclePtr& vehicle)     { m_units.emplace_back(vehicle); }

    void update();
    
    bool isPathFree(const Point& to, const VehicleGroup& obstacle, double iterationSize) const;
    bool canMoveRectTo(const Point& toCenter, const VehicleGroup& obstacleRect, double iterationSize) const;

    bool willCollide(const Vec2d& thisDisplacement, const VehicleGroup& other) const;

    VehicleGroup getGhost(const Vec2d& displacement) const
    {
		std::vector<VehicleCache> ghostUnits = m_units;
		todo - displacement for units;

        // TODO - safe code using inherited VehicleGroupGhost!!!
        return VehicleGroup{ std::vector<VehicleCache>(), m_center + displacement, m_rect + displacement };
    }
};

