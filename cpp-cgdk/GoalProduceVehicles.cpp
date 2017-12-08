#include "GoalProduceVehicles.h"
#include "noReleaseAssert.h"

using namespace goals;
using namespace model;

ProduceVehicles::ProduceVehicles(State& worldState, GoalManager& goalManager)
	: Goal(worldState, goalManager)
{
	// start vehicle production and then merge produced vehicles into existing group

	pushBackStep([this]() { return shouldAbort(); }, [this] { return shouldStartProdiction(); }, 
	             [this]() { return startProduction(); }, 
	             "start production", StepType::ALLOW_MULTITASK);
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
		[this](const auto* facility) { return facility->getOwnerPlayerId() != state().player()->getId(); });

	if (newEnd != sortedFacilities.end())
		sortedFacilities.erase(newEnd, sortedFacilities.end());

	static const Point myBase = { 100, 100 };

	std::sort(sortedFacilities.begin(), sortedFacilities.end(), [this](const Facility* a, const Facility* b) 
		{ return myBase.getSquareDistance(getFacilityCenter(a)) < myBase.getSquareDistance(getFacilityCenter(b)); });

	auto foundFactory = std::find_if(sortedFacilities.begin(), sortedFacilities.end(), [](const model::Facility* f) { return f->getType() == FacilityType::VEHICLE_FACTORY; });

	return foundFactory != sortedFacilities.end() ? *foundFactory : nullptr;
}
