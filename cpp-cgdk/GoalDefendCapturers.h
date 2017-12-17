#pragma once
#include "goal.h"
#include "forwardDeclarations.h"
#include "geometry.h"

namespace goals
{
    class DefendCapturers 
        : public Goal
    {
        virtual bool isCompatibleWith(const Goal* interrupted) override;

        bool shouldAbort();
        bool hasActionPoints() const { return state().hasActionPoint(); }

        bool moveHelicopters();

        struct ProtectionTarget
        {
            const  VehicleGroup* m_teammate;
            const  VehicleGroup* m_alliens;
            double               m_squareDistance;

            ProtectionTarget(const VehicleGroup& teammate, const VehicleGroup& alliens);
            ProtectionTarget(const VehicleGroup& teammate, double sqDistanceToTeammate) : m_teammate(&teammate), m_alliens(nullptr), m_squareDistance(sqDistanceToTeammate) {}

            ProtectionTarget() : m_teammate(nullptr), m_alliens(nullptr), m_squareDistance(0) {}
        };

        ProtectionTarget getProtectionTarget() const;
        Point            getProtectionPoint(const ProtectionTarget& protectionInfo) const;

    public:
        DefendCapturers(State& state, GoalManager& goalManager);
        ~DefendCapturers();

    };
}


