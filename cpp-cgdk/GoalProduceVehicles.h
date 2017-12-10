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

        size_t newUnitsCount() const;
        static const size_t MERGE_THRESHOLD = 100;

		bool shouldAbort() const           { return false; }
		bool hasActionPoints() const       { return state().hasActionPoint(); }
		bool shouldStartProdiction() const { return hasActionPoints() && getNearestFacility() != nullptr; }
        bool shouldMergeNewUnits() const   { return hasActionPoints() && newUnitsCount() >= MERGE_THRESHOLD; }

		Point getFacilityCenter(const model::Facility* facility) const;

		const model::Facility* getNearestFacility() const;

        const VehicleGroup& getGroupToMergeTo() const;
        double getMaxSpeed() const;


        State::GroupByType m_currentPortion;

		// actions

		bool startProduction();
		bool mergeToGroup();

	public:
		ProduceVehicles(State& worldState, GoalManager& goalManager);
		~ProduceVehicles();

	};
}

