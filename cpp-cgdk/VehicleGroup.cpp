#include "VehicleGroup.h"
#include <cassert>

void VehicleGroup::update()
{
	auto eraseIt = std::remove_if(m_units.begin(), m_units.end(), [](const VehicleCache& unitCache) { return unitCache.expired(); });
	if (eraseIt != m_units.end())
		m_units.erase(eraseIt, m_units.end());

	Point center;
	Rect  rect = m_units.empty() ? Rect() : Rect(*m_units.front().lock(), *m_units.front().lock());
	for (const VehicleCache& cache : m_units)
	{
		assert(!cache.expired());

		Point unitPoint = *cache.lock();
		center += unitPoint;
		rect.ensureContains(unitPoint);
	}

	center /= static_cast<double>(m_units.size());

	m_center = center;
	m_rect = rect;
}

bool VehicleGroup::mayIntersect(const VehicleGroup& other, const Vec2d& thisSpeed, const Vec2d& otherSpeed) const
{
	Rect myNextRect    = Rect(m_rect.m_topLeft + thisSpeed,        m_rect.m_bottomRight + thisSpeed);
	Rect otherNextRect = Rect(other.m_rect.m_topLeft + otherSpeed, other.m_rect.m_bottomRight + otherSpeed);

	return m_rect.overlaps(other.m_rect) || myNextRect.overlaps(otherNextRect);
}
