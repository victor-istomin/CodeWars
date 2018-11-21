#pragma once
#include "state.h"
#include "PotentialField.h"
#include "RewindClient.h"
#include "geometry.h"

#include <string>
#include <map>

class DebugOut
{
public:
    DebugOut();
    ~DebugOut();

    void drawVehicles(const State::VehicleByID& vehicles, const model::Player& me);
    void drawNuke(const Point& nuke, const VehiclePtr& guide, const State& state);

    template <typename TPotentialField, typename ValToColor>
    void drawPotentialField(const Point& dxdy, const TPotentialField& field, size_t layer, ValToColor&& getColor)
    {
#ifdef VISUALIZER
        field.visit([&](const auto& index, int score, const auto& pf)
        {
            Point topLeft     = pf.cellTopLeftToWorld(index);
            Point bottomRight = topLeft + Point { (double)pf.cellWidth(), (double)pf.cellHeight() };

            RewindClient::instance().rect(topLeft.m_x, topLeft.m_y, bottomRight.m_x, bottomRight.m_y, getColor(score), layer);
        });
#endif
    }

    void commitFrame();

};

class DebugTimer
{
    struct Info
    {
        size_t   events;
        uint32_t totalTime;
    };

    std::map<std::string, Info> m_events;

    DebugTimer() = default;
    ~DebugTimer();

public:

    void addEvent(const char* name, uint32_t time)
    {
        m_events[name].events++;
        m_events[name].totalTime += time;
    }

    static DebugTimer& instance()
    {
        static DebugTimer timer;
        return timer;
    }
};


