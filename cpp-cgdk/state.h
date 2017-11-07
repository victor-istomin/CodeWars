#pragma once
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <utility>
#include <cassert>
#include <memory>

#include "model/World.h"
#include "model/Player.h"


typedef std::shared_ptr<model::Vehicle> VehiclePtr;
typedef std::weak_ptr<model::Vehicle>   VehicleCache;

struct Point 
{ 
    double m_x;
    double m_y;

    Point(int x = 0, int y = 0)             : m_x(x), m_y(y)                     {}
    Point(const model::Unit& unit)          : m_x(unit.getX()), m_y(unit.getY()) {}

    Point& operator+=(const Point& right)   { m_x += right.m_x; m_y += right.m_y; return *this; }
    Point& operator/=(double divider)       { m_x /= divider; m_y /= divider; return *this; }
};

struct VehicleGroup
{
    std::vector<VehicleCache> m_units;
    Point                     m_center;

    void add(const VehiclePtr& vehicle)     { m_units.emplace_back(vehicle); }

    void update()
    {
        auto eraseIt = std::remove_if(m_units.begin(), m_units.end(), [](const VehicleCache& unitCache) { return unitCache.expired(); });
        if (eraseIt != m_units.end())
            m_units.erase(eraseIt, m_units.end());

        Point center;
        std::for_each(m_units.begin(), m_units.end(), [&center](const VehicleCache& cache) {center += *cache.lock(); });
        center /= m_units.size();

        m_center = center;
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

