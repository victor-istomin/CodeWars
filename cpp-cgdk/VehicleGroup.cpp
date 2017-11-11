#include "VehicleGroup.h"
#include <cassert>

void VehicleGroup::update()
{
    auto eraseIt = std::remove_if(m_units.begin(), m_units.end(), [](const VehicleCache& unitCache) { return unitCache.expired(); });
    if (eraseIt != m_units.end())
        m_units.erase(eraseIt, m_units.end());

    Point  center;
    double healthSum = 0;
    Rect   rect = m_units.empty() ? Rect() : Rect(*m_units.front().lock(), *m_units.front().lock());
    
    for (const VehicleCache& unitCache : m_units)
    {
        assert(!unitCache.expired());
        const model::Vehicle& unit = *unitCache.lock();

        Point unitPoint = unit;
        rect.ensureContains(unitPoint);
        center += unitPoint;

        healthSum += unit.getDurability();
    }

    center /= static_cast<double>(m_units.size());

    m_center    = center;
    m_rect      = rect.inflate(m_units.empty() ? 0.0 : m_units.front().lock()->getRadius());
    m_healthSum = healthSum;
}

