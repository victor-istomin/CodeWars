#pragma once
#include "state.h"
#include "PotentialField.h"
#include "RewindClient.h"
#include "geometry.h"

class DebugOut
{
public:
    DebugOut();
    ~DebugOut();

    void drawVehicles(const State::VehicleByID& vehicles, const model::Player& me);
    void drawNuke(const Point& nuke, const VehiclePtr& guide, const State& state);
    void drawRect(const Rect& rect, uint32_t color);

    template <typename TPotentialField, typename ValToColor>
    void drawPotentialField(const TPotentialField& field, size_t layer, ValToColor&& getColor)
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


