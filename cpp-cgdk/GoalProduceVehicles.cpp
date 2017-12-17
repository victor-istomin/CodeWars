#include "GoalProduceVehicles.h"
#include "noReleaseAssert.h"
#include "goalUtils.h"
#include <numeric>

using namespace goals;
using namespace model;

const size_t ProduceVehicles::MERGE_THRESHOLD = 30;


ProduceVehicles::ProduceVehicles(State& worldState, GoalManager& goalManager)
	: Goal(worldState, goalManager)
{
	// start vehicle production and then merge produced vehicles into existing group

	pushBackStep([this]() { return shouldAbort(); }, [this] { return shouldStartProdiction(); }, 
	             [this]() { return startProduction(); }, 
	             "start production", StepType::ALLOW_MULTITASK);

    pushBackStep([this]() { return shouldAbort(); }, [this] { return shouldMergeNewUnits(); },
                 [this]() { return mergeToGroup(); },
                 "merge new units", StepType::ALLOW_MULTITASK);
}


ProduceVehicles::~ProduceVehicles()
{
}

bool ProduceVehicles::isCompatibleWith(const Goal*)
{
	return true;   // looks compatible with any goal
}

bool ProduceVehicles::startProduction()
{
	const Facility* nearest = getNearestFacility();
	assert(nearest != nullptr);
	if (nearest == nullptr)
		return false;

	state().setProduceAction(nearest->getId(), VehicleType::HELICOPTER);

	return true;
}

Point ProduceVehicles::getFacilityCenter(const model::Facility* facility) const
{
	static const Point centerDisplacement = { state().game()->getFacilityWidth() / 2, state().game()->getFacilityHeight() / 2 };
	Point center = Point(facility->getLeft(), facility->getTop()) + centerDisplacement;

	return center;
}

const model::Facility* ProduceVehicles::getNearestFacility() const
{
	const auto& facilities = state().facilities();

	std::vector<const Facility*> sortedFacilities;
	sortedFacilities.reserve(facilities.size());
	std::transform(facilities.begin(), facilities.end(), std::back_inserter(sortedFacilities), [](const auto& idFacilityPair) { return &idFacilityPair.second; });

	auto newEnd = std::remove_if(sortedFacilities.begin(), sortedFacilities.end(),
		[this](const auto* facility) { return facility->getOwnerPlayerId() != this->state().player()->getId(); });

	if (newEnd != sortedFacilities.end())
		sortedFacilities.erase(newEnd, sortedFacilities.end());

	static const Point myBase = { 100, 100 };

	std::sort(sortedFacilities.begin(), sortedFacilities.end(), [this](const Facility* a, const Facility* b) 
		{ return myBase.getSquareDistance(getFacilityCenter(a)) < myBase.getSquareDistance(getFacilityCenter(b)); });

	auto foundFactory = std::find_if(sortedFacilities.begin(), sortedFacilities.end(), [](const model::Facility* f) { return f->getType() == FacilityType::VEHICLE_FACTORY; });

	return foundFactory != sortedFacilities.end() ? *foundFactory : nullptr;
}

bool ProduceVehicles::mergeToGroup()
{
    if (m_currentPortion.empty())
        m_currentPortion = state().popNewUnits();

    Rect vehiclesRect;
    for (auto& idGroupPair : m_currentPortion)
        idGroupPair.second.update();

    State::updateGroupsRect(m_currentPortion, vehiclesRect);

    state().setSelectAction(vehiclesRect);

    const VehicleGroup& mergeTo = getGroupToMergeTo();
    bool shouldMergeNow = mergeTo.m_units.empty();      // TODO - alternate strategy when merging into group of 1-5 units

    if (!shouldMergeNow)     
    {
        shouldMergeNow = vehiclesRect.overlaps(mergeTo.m_rect);
        if (!shouldMergeNow)
        {
            Vec2d moveVector = mergeTo.m_center - vehiclesRect.center();

            int ticksGap = std::max(10, static_cast<int>(moveVector.length() / getMaxSpeed() / 2));

            // LIFO pushing order!

            pushNextStep([this]() { return shouldAbort(); }, [this]() { return hasActionPoints(); }, [this]() { return mergeToGroup(); },
                         "looping merge move", StepType::ALLOW_MULTITASK);

            pushNextStep([this] { return shouldAbort(); }, WaitSomeTicks(state(), ticksGap), DoNothing(), "wait looping step", StepType::ALLOW_MULTITASK);

            pushNextStep([this]() { return shouldAbort(); }, [this]() { return hasActionPoints(); },
                         [this, moveVector]() { state().setMoveAction(moveVector); return true; },
                         "move new vehicles");
        }
    }

    if (shouldMergeNow)
    {
        state().mergeNewUnits(m_currentPortion);
        m_currentPortion.clear();

        pushBackStep([this]() { return shouldAbort(); }, [this] { return shouldMergeNewUnits(); },
                     [this]() { return mergeToGroup(); },
                     "next merge new units", StepType::ALLOW_MULTITASK);
    }

    return true;
}

double ProduceVehicles::getMaxSpeed() const
{
    double maxSpeed = 0.001;
    for (const auto& idVehiclePair : m_currentPortion)
        if (!idVehiclePair.second.m_units.empty())
            maxSpeed = std::max(maxSpeed, idVehiclePair.second.m_units.front().lock()->getMaxSpeed());

    return maxSpeed;
}

const VehicleGroup& goals::ProduceVehicles::getGroupToMergeTo() const
{
    std::list<const VehicleGroup*> groups;
    std::transform(state().teammates().begin(), state().teammates().end(), std::back_inserter(groups), 
        [](const auto& idGroupPair) { return &idGroupPair.second; });

    groups.sort([](const VehicleGroup* a, const VehicleGroup* b) { return a->m_units.size() > b->m_units.size(); });

    static const VehicleGroup k_null = VehicleGroup();

    return !groups.empty() ? *groups.front() : k_null;
}
