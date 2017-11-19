#pragma once
#include <memory>
#include <vector>
#include "geometry.h"

#include "model/Vehicle.h"

typedef std::shared_ptr<model::Vehicle> VehiclePtr;
typedef std::weak_ptr<model::Vehicle>   VehicleCache;

class Obstacle;

struct VehicleGroup
{
    std::vector<VehicleCache> m_units;
    Point  m_center;
    Rect   m_rect;
    double m_healthSum;
	double m_maxUnitRadius;

    void add(const VehiclePtr& vehicle)     { m_units.emplace_back(vehicle); }

    void update();
    
    bool isPathFree(const Point& to, const Obstacle& obstacle, double iterationSize, bool isRoughCalculation = false) const;
    bool willCollide(const Vec2d& thisDisplacement, const Obstacle& other, bool isRoughCalculation) const;
};

struct VehicleGroupGhost
{
    const VehicleGroup& m_original;
    std::vector<Point>  m_unitPlaces;
    Point m_center;
    Rect  m_rect;

    VehicleGroupGhost(const VehicleGroup& group, const Vec2d& displacement)
        : m_original(group)
        , m_unitPlaces()
        , m_center(group.m_center + displacement)
        , m_rect(group.m_rect + displacement)
    {
        const std::vector<VehicleCache>& units = m_original.m_units;
        m_unitPlaces.reserve(units.size());

        for (const VehicleCache& unitCache : units)
            m_unitPlaces.emplace_back(Point(*unitCache.lock()) + displacement);
    }

    // TODO- remove duplication with VehicleGroup
    bool isPathFree(const Point& to, const Obstacle& obstacle, double iterationSize, bool isRoughCalculation = false) const;
    bool willCollide(const Vec2d& thisDisplacement, const Obstacle& other, bool isRoughCalculation) const;


    VehicleGroupGhost(VehicleGroupGhost&&) = default;
    VehicleGroupGhost& operator=(VehicleGroupGhost&&) = default;

    VehicleGroupGhost(const VehicleGroupGhost&) = delete;
    VehicleGroupGhost& operator=(const VehicleGroupGhost&) = delete;
};

class Obstacle
{
    static const VehicleGroupGhost s_nullGhost;

    const bool               m_isReal;
    const VehicleGroup&      m_real;
    const VehicleGroupGhost& m_ghost;

public:

    explicit Obstacle(const VehicleGroup&      real)  : m_isReal(true),  m_real(real),             m_ghost(s_nullGhost) {}
    explicit Obstacle(const VehicleGroupGhost& ghost) : m_isReal(false), m_real(ghost.m_original), m_ghost(ghost)       {}

    const Point& center() const { return m_isReal ? m_real.m_center : m_ghost.m_center; }
    const Rect&  rect() const   { return m_isReal ? m_real.m_rect   : m_ghost.m_rect; }

    double maxUnitRadius() const { return m_real.m_maxUnitRadius; }

    size_t getUnitsCount() const         { return m_real.m_units.size(); }
    Point  getUnitPoint(size_t i) const  { return m_isReal ? Point(*m_real.m_units[i].lock()) : m_ghost.m_unitPlaces[i]; }
    double getUnitRadius(size_t i) const { return m_real.m_units[i].lock()->getRadius(); }
};

