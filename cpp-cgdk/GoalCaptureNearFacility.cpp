#include "GoalCaptureNearFacility.h"

using namespace goals;
using namespace model;

CaptureNearFacility::CaptureNearFacility(State& worldState, GoalManager& goalManager)
    : Goal(worldState, goalManager)
{
}


CaptureNearFacility::~CaptureNearFacility()
{
}
