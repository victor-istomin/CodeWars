#pragma once
#include "VehicleGroup.h"

namespace goals
{
	class WaitUntilStops
	{
		const VehicleGroup& m_group;
		Rect  m_previousRect;
		Point m_previousCenter;

	public:
		explicit WaitUntilStops(const VehicleGroup& group) : m_group(group), m_previousRect(Point(-1, -1), Point(-1, -1)) {}

		bool operator()()
		{
			bool isStopped = m_previousRect == m_group.m_rect && m_previousCenter == m_group.m_center;

			m_previousRect = m_group.m_rect;
			m_previousCenter = m_group.m_center;
			return isStopped;
		}
	};

	struct NeverAbort { bool operator()() { return false; } };
	struct DoNothing { bool operator()() { return true; } };

	struct WaitMove
	{
		const VehicleGroup& group;
		Point destination;
		bool operator()() { return group.m_center == destination; }
	};

}

