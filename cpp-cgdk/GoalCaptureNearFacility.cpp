#include "GoalCaptureNearFacility.h"
#include "goalUtils.h"

using namespace goals;
using namespace model;

CaptureNearFacility::CaptureNearFacility(State& worldState, GoalManager& goalManager)
    : Goal(worldState, goalManager)
{
    pushBackStep([this]() {return shouldAbort(); }, WaitUntilStops(tankGroup()), DoNothing(), "wait until tank stops", StepType::ALLOW_MULTITASK);
    pushBackStep([this]() {return shouldAbort(); }, WaitUntilStops(ifvGroup()), DoNothing(), "wait until IFV stops", StepType::ALLOW_MULTITASK);
    pushBackStep([this]() {return shouldAbort(); }, WaitUntilStops(arrvGroup()), DoNothing(), "wait until arrv stops", StepType::ALLOW_MULTITASK);

    pushBackStep([this]() { return shouldAbort(); }, [this]() { return hasActionPoints(); }, [this]() { return createMixedGroup(); }, "assign group #");

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
            return false;   // nothing to capture, abort. TODO: better implementation

        pushBackStep([this]() {return shouldAbort(); },
                     [this]() {return hasActionPoints(); },
                     [this, groupId, targetId]() { return performCapture(groupId, targetId);}, "perform facility capture");
    }

    pushBackStep([this]() {return shouldAbort(); }, [this]() {return hasActionPoints(); }, [this]() { return startCapture(); }, "continue capturing");

    return true;
}

State::Id CaptureNearFacility::getNearestFacility(const VehicleGroup& teammate)
{
    const auto& facilities = state().facilities();

    std::vector<const Facility*> sortedFacilities;
    sortedFacilities.reserve(facilities.size());
    std::transform(facilities.begin(), facilities.end(), std::back_inserter(sortedFacilities), [](const auto& idFacilityPair) { return &idFacilityPair.second; });

    auto newEnd = std::remove_if(sortedFacilities.begin(), sortedFacilities.end(), [this](const model::Facility* facility) {
        return facility->getOwnerPlayerId() != ID_NONE;   // already captured
    });

    if (newEnd != sortedFacilities.end())
        sortedFacilities.erase(newEnd, sortedFacilities.end());

    if (sortedFacilities.empty())
    {
        // fallback to capturing enemy's facilities. Don't capture factories, because there may be a lot of new vehicles

        std::transform(facilities.begin(), facilities.end(), std::back_inserter(sortedFacilities), [](const auto& idFacilityPair) { return &idFacilityPair.second; });
        newEnd = std::remove_if(sortedFacilities.begin(), sortedFacilities.end(), [this](const model::Facility* facility) 
        {
            return facility->getOwnerPlayerId() == state().player()->getId();
        });

        if (newEnd != sortedFacilities.end())
            sortedFacilities.erase(newEnd, sortedFacilities.end());
    }

    Point groupPosition = teammate.m_center;
    std::sort(sortedFacilities.begin(), sortedFacilities.end(), [this, &groupPosition](const Facility* a, const Facility* b) 
    { 
        Point aCenter = getFacilityCenter(a);
        Point bCenter = getFacilityCenter(b);
        return groupPosition.getSquareDistance(aCenter) < groupPosition.getSquareDistance(bCenter); 
    });

    // todo: check if path is free

    return sortedFacilities.empty() ? ID_NONE : sortedFacilities.front()->getId();    
}

CaptureNearFacility::GroupsList CaptureNearFacility::getGroupsForCapture()
{
    return { VehicleType::TANK };  // TODO
}

bool CaptureNearFacility::performCapture(GroupId performerId, State::Id facilityId)
{
    const VehicleGroup& performer = state().teammates(performerId);

    const Facility* facility = state().facility(facilityId);
    Vec2d moveVector = getFacilityCenter(facility) - performer.m_center;

    state().setSelectAction(State::GROUP_CAPTURING);

    if (moveVector.length() > 1)
    {
        // LIFO pushing order!
        static const int PAUSE_TICKS = std::max(10, static_cast<int>(moveVector.length() / state().game()->getTankSpeed() / 2));  // TODO
        pushNextStep([this]() {return shouldAbort(); }, WaitSomeTicks(state(), PAUSE_TICKS), DoNothing(), "wait until next move", StepType::ALLOW_MULTITASK);

        pushNextStep([this]() {return shouldAbort(); }, WaitUntilStops(tankGroup()), DoNothing(), "wait until tank stops", StepType::ALLOW_MULTITASK);
        pushNextStep([this]() {return shouldAbort(); }, WaitUntilStops(ifvGroup()),  DoNothing(), "wait until IFV stops", StepType::ALLOW_MULTITASK);
        pushNextStep([this]() {return shouldAbort(); }, WaitUntilStops(arrvGroup()), DoNothing(), "wait until arrv stops", StepType::ALLOW_MULTITASK);

        pushNextStep([this]() {return shouldAbort(); },
                     [this]() {return hasActionPoints(); },
                     [this, moveVector]() { state().setMoveAction(moveVector, state().game()->getTankSpeed()); return true; }, "capture move");
    }
    else
    {
        // just wait
        pushNextStep([this]() {return shouldAbort(); }, WaitSomeTicks(state(), 10), DoNothing(), "wait until next move", StepType::ALLOW_MULTITASK);
    }

    return true;
}

Point CaptureNearFacility::getFacilityCenter(const model::Facility* facility) const
{
    static const Point centerDisplacement = { state().game()->getFacilityWidth() / 2, state().game()->getFacilityHeight() / 2 };
    Point center = Point(facility->getLeft(), facility->getTop()) + centerDisplacement;

    return center;
}

bool CaptureNearFacility::createMixedGroup()
{
    const VehicleGroup& performer = tankGroup();

    Rect selectionRect = performer.m_rect;
    for (const auto& idGroupPair : state().teammates())
    {
        const auto&         id    = idGroupPair.first;
        const VehicleGroup& group = idGroupPair.second;

        if (id == VehicleType::FIGHTER || id == VehicleType::HELICOPTER)    // aerials have their own goal
            continue;

        if (selectionRect.overlaps(group.m_rect))
            selectionRect.ensureContains(group.m_rect);
    }

    // LIFO pushing order: select tanks, add IFV, add ARRV and then assign group number

    pushNextStep([this]() {return shouldAbort(); },
                 [this]() {return hasActionPoints(); },
                 [this]() {state().setAssignGroupAction(State::GROUP_CAPTURING); return true; }, "create capturing group");

    pushNextStep([this]() {return shouldAbort(); },
                 [this]() {return hasActionPoints(); },
                 [this, selectionRect]() { state().setAddSelectionAction(selectionRect, VehicleType::ARRV); return true; }, "add arrv to selection");

    pushNextStep([this]() {return shouldAbort(); },
                 [this]() {return hasActionPoints(); },
                 [this, selectionRect]() { state().setAddSelectionAction(selectionRect, VehicleType::IFV); return true; }, "add IFV to selection");

    state().setSelectAction(selectionRect, VehicleType::TANK);

    return true;
}
