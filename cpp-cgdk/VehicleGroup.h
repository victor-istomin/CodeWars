#pragma once
#include <memory>
#include <vector>
#include <functional>
#include "geometry.h"

#include "model/Vehicle.h"

typedef std::shared_ptr<model::Vehicle> VehiclePtr;
typedef std::weak_ptr<model::Vehicle>   VehicleCache;

class Obstacle;

class GroupHandle
{
public:

    enum class Type
    {
        eUNKNOWN = 0,
        eINITIAL,      // group of initially spawned units
        eARTIFICIAL,   // group of produced units
    };

    static GroupHandle initial   (model::VehicleType vehicleType)                         { return GroupHandle(Type::eINITIAL, vehicleType); }
    static GroupHandle initial   (const model::Vehicle& vehicle)                          { return initial(vehicle.getType()); }
    static GroupHandle artificial(model::VehicleType vehicleType)                         { return GroupHandle(Type::eARTIFICIAL, vehicleType); }
    static GroupHandle artificial(const model::Vehicle& vehicle)                          { return artificial(vehicle.getType()); }

    model::VehicleType vehicleType() const                                                { return m_vehicleType; }
    Type               type() const                                                       { return m_groupType; }

    bool operator<(const GroupHandle& right) const 
    { 
        return m_vehicleType < right.m_vehicleType || (m_vehicleType == right.m_vehicleType && m_groupType < right.m_groupType);
    }

    bool operator==(const GroupHandle& right) const
    {
        return m_vehicleType == right.m_vehicleType && m_groupType == right.m_groupType;
    }

    bool operator!=(const GroupHandle& right) const
    {
        return !(*this == right);
    }

private:

    Type               m_groupType;
    model::VehicleType m_vehicleType;

    GroupHandle(Type groupType, model::VehicleType vehilceType) : m_groupType(groupType), m_vehicleType(vehilceType) {}

};

namespace std
{
    template <>
    struct hash<GroupHandle>
    {
        size_t operator()(const GroupHandle& k) const
        {
            // Compute individual hash values for first, second and third
            // http://stackoverflow.com/a/1646913/126995
            size_t res = 17;
            res = res * 31 + hash<int>()(static_cast<int>(k.type()));
            res = res * 31 + hash<int>()(static_cast<int>(k.vehicleType()));
            return res;
        }
    };
}

struct VehicleGroup
{
    std::vector<VehicleCache> m_units;
    Point  m_center;
    Rect   m_rect;
    double m_healthSum;
    double m_maxUnitRadius;
    Point  m_plannedDestination;

    // TODO refactor: add getters/setters
    void  setPlannedDestination(const Point& dest = Point()) { m_plannedDestination = dest; }
    bool  hasPlannedDestination() const                      { return m_plannedDestination != Point(0,0); }
    Point getPredictedCenter() const                         { return hasPlannedDestination() ? m_plannedDestination : m_center; }

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

