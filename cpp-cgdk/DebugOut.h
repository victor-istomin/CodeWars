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

    template <typename TPotentialField, typename ValToColor>
    void drawPotentialField(const Point& dxdy, const TPotentialField& field, size_t layer, ValToColor&& getColor)
    {
#ifdef VISUALIZER
        field.visit([&](int x, int y, int score, const auto& dummy)
        {
            Point topLeft     = dxdy    + Point { (double)x * field.cellWidth(), (double)y * field.cellHeight() };
            Point bottomRight = topLeft + Point { (double)field.cellWidth(),     (double)field.cellHeight() };

            RewindClient::instance().rect(topLeft.m_x, topLeft.m_y, bottomRight.m_x, bottomRight.m_y, getColor(score), layer);
        });
#endif
    }

    void commitFrame();

};

