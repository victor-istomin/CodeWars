#pragma once
#include <list>

#include "goal.h"
#include "forwardDeclarations.h"
#include "VehicleGroup.h"

namespace goals
{
    class CaptureNearFacility 
        : public Goal
    {
        typedef std::list<GroupId> GroupsList;

        static const int ID_NONE = -1;

        typedef std::unordered_map<GroupId, State::Id>  GroupTargets;
        GroupTargets m_actualTargets;

        GroupsList getGroupsForCapture();
        State::Id  getNearestFacility(const VehicleGroup& teammates);

        Point getFacilityCenter(const model::Facility* facility) const;

        bool shouldAbort() const        { return false; }
        bool hasActionPoints() const    { return state().hasActionPoint(); }

        // actions

        bool createMixedGroup();
        bool startCapture();
        bool performCapture(GroupId performer, State::Id facilityId);

    public:
        CaptureNearFacility(State& state, GoalManager& goalManager);
        ~CaptureNearFacility();


    };
}

