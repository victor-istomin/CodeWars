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
    m_rect   = rect;
}

