#include "VehicleGroup.h"
#include <cassert>

void VehicleGroup::update()
{
    auto eraseIt = std::remove_if(m_units.begin(), m_units.end(), [](const VehicleCache& unitCache) { return unitCache.expired(); });
    if (eraseIt != m_units.end())
        m_units.erase(eraseIt, m_units.end());

    Point  center;
    double healthSum = 0;
    double maxRadius = 0;

    Rect   rect = m_units.empty() ? Rect() : Rect(*m_units.front().lock(), *m_units.front().lock());
    
    for (const VehicleCache& unitCache : m_units)
    {
        assert(!unitCache.expired());
        const VehiclePtr unit = unitCache.lock();

        Point unitPoint = *unit;
        rect.ensureContains(unitPoint);
        center += unitPoint;

        healthSum += unit->getDurability();

		maxRadius = std::max(maxRadius, unit->getRadius());
    }

    center /= static_cast<double>(m_units.size());

    m_center        = center;
    m_rect          = rect.inflate(m_units.empty() ? 0.0 : m_units.front().lock()->getRadius());
    m_healthSum     = healthSum;
	m_maxUnitRadius = maxRadius;
}

bool VehicleGroup::isPathFree(const Point& to, const VehicleGroup& obstacle, double iterationSize) const
{
    const Point& groupCenter    = m_center;
    const Point& obstacleCenter = obstacle.m_center;

    // TODO: BUG - get angle between closest rect corners, not between centers
    double angleBetween = std::abs(Vec2d::angleBetween((to - groupCenter), (obstacleCenter - groupCenter)));

//     if (groupCenter.getDistanceTo(obstacleCenter) > groupCenter.getDistanceTo(to) || angleBetween > (PI / 2))
//     {
//         return true;   // mid-air collision is unlikely
//     }

    return canMoveRectTo(to, obstacle, iterationSize);
}

bool VehicleGroup::canMoveRectTo(const Point& toCenter, const VehicleGroup& obstacle, double iterationSize) const
{
    // TODO - it's possible to perform more careful check
    Vec2d  direction = Vec2d::fromPoint(toCenter - m_center).truncate(iterationSize);
    double distance  = m_center.getDistanceTo(toCenter);

    bool  isPathFree = true;
    Vec2d move = direction;
    do
    {
        //Rect destination = fromRect + move;   // for debug purposes
        //isPathFree = !destination.overlaps(obstacle.m_rect);
        isPathFree = !willCollide(move, obstacle);
        move += direction;
    } while (move.length() < distance && isPathFree);

	// also check final destination:
	if (isPathFree)
		isPathFree = !willCollide(move.truncate(distance), obstacle);

    return isPathFree;
}

bool VehicleGroup::willCollide(const Vec2d& thisDisplacement, const VehicleGroup& other) const
{
	Rect intersection;
	Rect actualRect = m_rect + thisDisplacement;
	if (!actualRect.overlaps(other.m_rect, intersection))
		return false;

	intersection = intersection.inflate(m_maxUnitRadius + other.m_maxUnitRadius);

	for (const VehicleCache& vehicleCache : m_units)
	{
		VehiclePtr vehicle = vehicleCache.lock();
		Point actualCenter = Point(*vehicle) + thisDisplacement;

		if (!intersection.contains(actualCenter))
			continue;

		for (const VehicleCache& otherGroupCache : other.m_units)
		{
			VehiclePtr otherVehicle = otherGroupCache.lock();

			if (!intersection.contains(*otherVehicle) || otherVehicle->getId() == vehicle->getId())
				continue;

			double squaredDistance = actualCenter.getSquareDistance(*otherVehicle);
			double sqaredRadius    = Point::pow2(vehicle->getRadius() + otherVehicle->getRadius());

			if (squaredDistance < sqaredRadius)
				return true; 
		}
	}

	return false;
}


