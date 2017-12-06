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
        typedef std::list<GroupId>                      GroupsList;

//         typedef std::unordered_map<GroupId, State::Id>  GroupTargets;
//         GroupTargets m_targets;

        bool       isActiveMode();          // active mode: 1 group - 1 target, instead: N groups - 1 target
        GroupsList getGroupsForCapture();
        State::Id  getNearestFacility(const VehicleGroup& teammates);

        bool shouldAbort() const        { return false; }
        bool hasActionPoints() const    { return state().hasActionPoint(); }

        // actions

        bool startCapture();
        bool performCapture(GroupId performer, State::Id facilityId);

    public:
        CaptureNearFacility(State& state, GoalManager& goalManager);
        ~CaptureNearFacility();


    };
}

