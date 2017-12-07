#include "GoalDefendCapturers.h"
#include "GoalCaptureNearFacility.h"

using namespace goals;
using namespace model;

DefendCapturers::DefendCapturers(State& worldState, GoalManager& goalManager)
    : Goal(worldState, goalManager)
{
}

DefendCapturers::~DefendCapturers()
{
}

bool DefendCapturers::isCompatibleWith(const Goal* interrupted)
{
    return nullptr != dynamic_cast<const CaptureNearFacility*>(interrupted);
}
