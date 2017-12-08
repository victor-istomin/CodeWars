#pragma once
#include "goal.h"
#include "forwardDeclarations.h"
#include "model/Facility.h"

namespace goals
{
	class ProduceVehicles :
		public Goal
	{
		virtual bool isCompatibleWith(const Goal* interrupted) override;

		bool shouldAbort() const { return false; }
		bool hasActionPoints() const { return state().hasActionPoint(); }
		bool shouldStartProdiction() const { return hasActionPoints() && getNearestFacility() != nullptr; }

		Point getFacilityCenter(const model::Facility* facility) const;

		const model::Facility* getNearestFacility() const;

		// actions

		bool startProduction();
		bool mergeToGroup();

	public:
		ProduceVehicles(State& worldState, GoalManager& goalManager);
		~ProduceVehicles();

	};
}

