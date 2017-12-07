#pragma once
#include <limits>

#include "goal.h"
#include "forwardDeclarations.h"

namespace goals
{
    class RushWithAircraft :
        public Goal
    {
        struct TargetInfo
        {
            model::VehicleType  m_type;
            const VehicleGroup* m_group;
            double              m_dangerFactor;
            double              m_minSqDistance;

            TargetInfo(const VehicleGroup& group)
                : m_group(&group)
                , m_type(group.m_units.empty() ? model::VehicleType::_UNKNOWN_ : group.m_units.front().lock()->getType())
                , m_dangerFactor(0)
                , m_minSqDistance(std::numeric_limits<double>::max())
            {}

            bool isEliminated() const { return m_type == model::VehicleType::_UNKNOWN_; }
        };

        static bool isAerial(model::VehicleType type) { return type == model::VehicleType::FIGHTER || type == model::VehicleType::HELICOPTER; }

        bool doNextFightersMove();

        bool validateMoveVector(Vec2d& moveVector);
        TargetInfo getFightersTargetInfo();

        virtual bool isCompatibleWith(const Goal* interrupted) override;

    public:
        RushWithAircraft(State& state, GoalManager& goalManager);
        ~RushWithAircraft();

        bool shouldAbort() const;
    };

}

