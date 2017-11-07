#pragma once
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <utility>
#include <cassert>
#include <memory>
#include <optional>

#include "model/World.h"
#include "model/Player.h"


typedef std::shared_ptr<model::Vehicle> VehiclePtr;
typedef std::weak_ptr<model::Vehicle>   VehicleCache;

struct Point 
{ 
    double m_x;
    double m_y;

    Point(double x = 0, double y = 0)       : m_x(x), m_y(y)                     {}
    Point(const model::Unit& unit)          : m_x(unit.getX()), m_y(unit.getY()) {}

    Point& operator+=(const Point& right)   { m_x += right.m_x; m_y += right.m_y; return *this; }
    Point& operator/=(double divider)       { m_x /= divider; m_y /= divider; return *this; }
    
    Point& operator+(const Point& right)    { return Point(*this) += right; }
};

typedef Point Vec2d;

struct Rect
{
    Point m_topLeft;
    Point m_bottomRight;
    
    Rect(const Point& topLeft = Point(), const Point& bottomRight = Point())
        : m_topLeft(topLeft), m_bottomRight(bottomRight) {}
    
    // check if rect overlaps with other one
    bool overlaps(const Rect& other) const
    {
        bool noOverlap = this->m_topLeft.m_x > other.m_bottomRight.m_x || other.m_topLeft.m_x > this->m_bottomRight.m_x
                      || this->m_topLeft.m_y > other.m_bottomRight.m_y || other.m_topLeft.m_y > this->m_bottomRight.m_y;
                       
        return !noOverlap;
    }
    
    bool overlaps(const Rect& rect, Rect& intersection)
    {
        bool doesOverlap = overlaps(rect);
        if(doesOverlap)
            intersection = Rect( {std::max(this->m_topLeft.m_x,     other.m_topLeft.m_x),     std::max(this->m_topLeft.m_y,     other.m_topLeft.m_y)}, 
                                 {std::min(this->m_bottomRight.m_x, other.m_bottomRight.m_x), std::min(this->m_bottomRight.m_y, other.m_bottomRight.m_y)});
        return doesOverlap;
    }
    
    // ensure 'inside' point is actually inside rect 
    void ensureContains(const Point& inside)
    {
        m_topLeft     = Point(std::min(m_topLeft.m_x,     inside.m_x), std::min(m_topLeft.m_y, inside.m_y));
        m_bottomRight = Point(std::max(m_bottomRight.m_x, inside.m_x), std::max(m_bottomRight.m_y, inside.m_y));
    }
};

struct VehicleGroup
{
    std::vector<VehicleCache> m_units;
    Point                     m_center;
    Rect                      m_rect;

    void add(const VehiclePtr& vehicle)     { m_units.emplace_back(vehicle); }

    void update()
    {
        auto eraseIt = std::remove_if(m_units.begin(), m_units.end(), [](const VehicleCache& unitCache) { return unitCache.expired(); });
        if (eraseIt != m_units.end())
            m_units.erase(eraseIt, m_units.end());

        Point center;
        Rect  rect = m_units.empty() ? Rect() : Rect(*m_units.front().lock(), *m_units.front().lock());
        for(const VehicleCache& cache : m_units)
        {
            assert(!cache.expired());
            
            Point unitPoint = *cache.lock();
            center += unitPoint;
            rect.ensureContains(unitPoint);
        }
            
        center /= m_units.size();

        m_center = center;
        m_rect   = rect;
    }
    
    bool mayIntersect(const VehicleGroup& other, const Vec2d& thisSpeed, const Vec2d& otherSpeed) const
    {
        Rect myNextRect    = Rect(m_rect.m_topLeft + thisSpeed,        m_rect.m_bottomRight + thisSpeed);
        Rect otherNextRect = Rect(other.m_rect.m_topLeft + otherSpeed, other.m_rect.m_bottomRight + otherSpeed);
        
        return m_rect.overlaps(other) || myNextRect.overlaps(otherNextRect);            
    }
};

struct State
{
    typedef std::map<int, VehiclePtr>                   VehicleByID;
    typedef std::map<model::VehicleType, VehicleGroup>  GroupByType;

    VehicleByID m_vehicles;
    GroupByType m_alliens;
    GroupByType m_teammates;

    bool                  hasVehicleById(int id) const { return m_vehicles.find(id) != m_vehicles.end(); }
    const model::Vehicle& vehicleById(int id)    const { return *m_vehicles.find(id)->second; }
    model::Vehicle&       vehicleById(int id)          { return *m_vehicles.find(id)->second; }

    void update(const model::World& world, const model::Player& me)
    {
        assert(me.isMe());

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
    }
};

