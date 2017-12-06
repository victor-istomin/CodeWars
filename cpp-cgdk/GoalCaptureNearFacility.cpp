#include "GoalCaptureNearFacility.h"

using namespace goals;
using namespace model;

CaptureNearFacility::CaptureNearFacility(State& worldState, GoalManager& goalManager)
    : Goal(worldState, goalManager)
{
    pushBackStep([this]() { return shouldAbort(); }, [this]() { return hasActionPoints(); }, [this]() { return startCapture(); }, "start capturing");
}

CaptureNearFacility::~CaptureNearFacility()
{
}

bool CaptureNearFacility::startCapture()
{
    auto activeGroups = getGroupsForCapture();
    for (GroupId groupId : activeGroups)
    {
        State::Id targetId = getNearestFacility(state().teammates(groupId));
        if (targetId == State::INVALID_ID)
            continue;

        pushBackStep([this]() {return shouldAbort(); },
                     [this]() {return hasActionPoints(); },
                     [this, groupId, targetId]() { return performCapture(groupId, targetId);}, "perform facility capture");
    }

    pushBackStep([this]() {return shouldAbort(); }, [this]() {return hasActionPoints(); }, [this]() { return startCapture(); }, "continue capturing");
}
