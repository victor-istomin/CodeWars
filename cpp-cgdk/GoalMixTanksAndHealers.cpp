#include "GoalMixTanksAndHealers.h"
#include "State.h"
#include "VehicleGroup.h"

#include <map>

using namespace goals;
using namespace model;

namespace
{
    enum class FormationOrder
    {
        eFT_UNKNOWN = 0,     // not valid
        dFT_IVF_HEALER_TANK,
        eFT_TANK_HEALER_IFV,
    };
}

MixTanksAndHealers::MixTanksAndHealers(State& worldState)
    : Goal(worldState)
{
    std::map<VehicleType, int> rows;
    std::map<VehicleType, int> columns;
    for (VehicleType type : {VehicleType::ARRV, VehicleType::IFV, VehicleType::TANK})
    {
        const VehicleGroup& group = state().alliens(type);
        rows   [type] = group.m_center.m_y / group.m_rect.height();
        columns[type] = group.m_center.m_x / group.m_rect.width();
    }

    bool isNeedIfvShift = rows[VehicleType::ARRV] == rows[VehicleType::IFV] && rows[VehicleType::ARRV] == rows[VehicleType::TANK];

    auto hasActionPoint = [this]() { return state().hasActionPoint(); };

    static const double GAP_FACTOR = 1.2;

    Point shiftPoint = ifvGroup().m_center + (Point(0, ifvGroup().m_rect.height()) * GAP_FACTOR);

    if (isNeedIfvShift)
    {
        auto isMoveCompleted = [this]() 
        {
            const VehicleGroup& ifv = ifvGroup(); 
            return ifv.getPredictedCenter() == ifv.m_center; 
        };

        pushNextStep(NeverAbort(), hasActionPoint, [this]() { return shiftIfv(); }, "mix: shift IFV");
        pushNextStep(NeverAbort(), isMoveCompleted, []

    }
}

MixTanksAndHealers::~MixTanksAndHealers()
{
}
