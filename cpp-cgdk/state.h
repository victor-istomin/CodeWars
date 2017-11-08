#pragma once
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <utility>
#include <cassert>
#include <memory>
#include <cmath>

#include "model/World.h"
#include "model/Player.h"
#include "model/Move.h"
#include "model/ActionType.h"

#undef max
#undef min

typedef std::shared_ptr<model::Vehicle> VehiclePtr;
typedef std::weak_ptr<model::Vehicle>   VehicleCache;

struct Point 
{ 
    static const double k_epsilon;
    static double pow2(double d) { return d*d; }

    double m_x;
    double m_y;

    Point(double x = 0, double y = 0)       : m_x(x), m_y(y)                     {}
    Point(const model::Unit& unit)          : m_x(unit.getX()), m_y(unit.getY()) {}

    Point& operator+=(const Point& right)   { m_x += right.m_x; m_y += right.m_y; return *this; }
    Point& operator-=(const Point& right)   { m_x -= right.m_x; m_y -= right.m_y; return *this; }
    Point& operator/=(double divider)       { m_x /= divider;   m_y /= divider;   return *this; }
    
    friend Point operator+(const Point& left, const Point& right)    { return Point(left) += right; }
    friend Point operator-(const Point& left, const Point& right)    { return Point(left) -= right; }


    bool operator==(const Point& right) const { return std::abs(m_x - right.m_x) < k_epsilon && std::abs(m_y - right.m_y) < k_epsilon; }

    // unused:
    // bool operator!=(const Point2D& right) const { return !this->operator ==(right); }
    // bool isZero() const { return *this == Point2D(0, 0); }

    double getDistanceTo(const Point& other)     const { return std::sqrt(getSquareDistance(other)); }
    double getSquareDistance(const Point& other) const { return pow2(m_x - other.m_x) + pow2(m_y - other.m_y); }  // sometimes we could compare Distance² with Radius² to omit expensive sqrt() and/or more expensive hypot()
};

__declspec(selectany) const double Point::k_epsilon = 0.0001;   // TODO - get epsilon from game system!

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
    
    bool overlaps(const Rect& other, Rect& intersection)
    {
        bool doesOverlap = overlaps(other);
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
            
        center /= static_cast<double>(m_units.size());

        m_center = center;
        m_rect   = rect;
    }
    
    bool mayIntersect(const VehicleGroup& other, const Vec2d& thisSpeed, const Vec2d& otherSpeed) const
    {
        Rect myNextRect    = Rect(m_rect.m_topLeft + thisSpeed,        m_rect.m_bottomRight + thisSpeed);
        Rect otherNextRect = Rect(other.m_rect.m_topLeft + otherSpeed, other.m_rect.m_bottomRight + otherSpeed);
        
        return m_rect.overlaps(other.m_rect) || myNextRect.overlaps(otherNextRect);            
    }
};

struct State
{
    typedef decltype(((model::Vehicle*)nullptr)->getId()) Id;
    typedef std::map<Id, VehiclePtr>                      VehicleByID;
    typedef std::map<model::VehicleType, VehicleGroup>    GroupByType;

    VehicleByID m_vehicles;
    GroupByType m_alliens;
    GroupByType m_teammates;
    bool        m_isMoveComitted;

    const model::World*  m_world;
    const model::Player* m_player;
    const model::Game*   m_game;
    model::Move*         m_move;

    bool                  hasVehicleById(Id id) const { return m_vehicles.find(id) != m_vehicles.end(); }
    const model::Vehicle& vehicleById(Id id)    const { return *m_vehicles.find(id)->second; }
    model::Vehicle&       vehicleById(Id id)          { return *m_vehicles.find(id)->second; }

    void update(const model::World& world, const model::Player& me, const model::Game& game, model::Move& move)
    {
        assert(me.isMe());
        m_world  = &world;
        m_player = &me;
        m_game   = &game;
        m_move   = &move;

        m_isMoveComitted = false;

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
    
    void setSelectAction(const Rect& rect, model::VehicleType vehicleType = model::_VEHICLE_UNKNOWN_)
    {
        m_move->setAction(model::ACTION_CLEAR_AND_SELECT);
        
        if (vehicleType != model::_VEHICLE_UNKNOWN_)
            m_move->setVehicleType(vehicleType);
        
        m_move->setTop(rect.m_topLeft.m_y);
        m_move->setLeft(rect.m_topLeft.m_x);
        m_move->setBottom(rect.m_bottomRight.m_y);
        m_move->setRight(rect.m_bottomRight.m_x);
        
        m_isMoveComitted = true;
    }
    
    void setMoveAction(const Vec2d& vector)
    {
        m_move->setAction(model::ACTION_MOVE);
        m_move->setX(vector.m_x);
        m_move->setY(vector.m_y);
        
        m_isMoveComitted = true;
    }
};

