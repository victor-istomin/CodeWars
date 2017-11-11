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
        const VehiclePtr unit = unitCache.lock();

        Point unitPoint = *unit;
        rect.ensureContains(unitPoint);
        center += unitPoint;

        healthSum += unit->getDurability();
    }

    center /= static_cast<double>(m_units.size());

    m_center    = center;
    m_rect      = rect.inflate(m_units.empty() ? 0.0 : m_units.front().lock()->getRadius());
    m_healthSum = healthSum;
}

bool VehicleGroup::isPathFree(const Point& to, const VehicleGroup& obstacle, double iterationSize) const
{
    const Point& groupCenter    = m_center;
    const Point& obstacleCenter = obstacle.m_center;

    // TODO: BUG - get angle between closest rect corners, not between centers
    double angleBetween = std::abs(Vec2d::angleBetween((to - groupCenter), (obstacleCenter - groupCenter)));

    if (groupCenter.getDistanceTo(obstacleCenter) > groupCenter.getDistanceTo(to) || angleBetween > (PI / 2))
    {
        return true;   // mid-air collision is unlikely
    }

    return canMoveRectTo(groupCenter, to, m_rect, obstacle.m_rect, iterationSize);
}

bool VehicleGroup::canMoveRectTo(const Point& from, const Point& to, const Rect& fromRect, const Rect& obstacleRect, double iterationSize)
{
    // TODO - it's possible to perform more careful check
    Vec2d direction = Vec2d::fromPoint(to - from).truncate(iterationSize);
    int stepsTotal = static_cast<int>(std::ceil(from.getDistanceTo(to) / iterationSize));

    bool isPathFree = true;

    for (int i = 0; i < stepsTotal && isPathFree; ++i)
    {
        Rect destination = fromRect + direction * i * iterationSize;
        isPathFree = !destination.overlaps(obstacleRect);
    }

    return isPathFree;
}

