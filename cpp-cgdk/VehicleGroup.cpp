#include "VehicleGroup.h"
#include "noReleaseAssert.h"

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

bool VehicleGroup::isPathFree(const Point& toCenter, const Obstacle& obstacle, double iterationSize, bool isRoughCalculation /*= false*/) const
{
    Vec2d  direction = Vec2d::fromPoint(toCenter - m_center).truncate(iterationSize);
    double distance = m_center.getDistanceTo(toCenter);

    bool  isPathFree = true;
    Vec2d move = direction;
    do
    {
        isPathFree = !willCollide(move, obstacle, isRoughCalculation);
        move += direction;
    } while (move.length() < distance && isPathFree);

    // also check final destination:
    if (isPathFree)
        isPathFree = !willCollide(move.truncate(distance), obstacle, isRoughCalculation);

    return isPathFree;
}

bool VehicleGroup::willCollide(const Vec2d& thisDisplacement, const Obstacle& other, bool isRoughCalculation) const
{
	Rect intersection;
	Rect actualRect = m_rect + thisDisplacement;
	if (!actualRect.overlaps(other.rect(), intersection))
		return false;

	intersection = intersection.inflate(m_maxUnitRadius + other.maxUnitRadius());

	for (const VehicleCache& vehicleCache : m_units)
	{
		VehiclePtr vehicle = vehicleCache.lock();
		Point actualCenter = Point(*vehicle) + thisDisplacement;

		if (!intersection.contains(actualCenter))
			continue;

        for(size_t i = 0; i < other.getUnitsCount(); ++i)
		{
            Point otherPoint = other.getUnitPoint(i);
			if (!intersection.contains(otherPoint))
				continue;

            if (isRoughCalculation)
                return true;

			double squaredDistance = actualCenter.getSquareDistance(otherPoint);
			double sqaredRadius    = Point::pow2(vehicle->getRadius() + other.getUnitRadius(i));

			if (squaredDistance < sqaredRadius)
				return true; 
		}
	}

	return false;
}


bool VehicleGroupGhost::isPathFree(const Point& toCenter, const Obstacle& obstacle, double iterationSize, bool isRoughCalculation /*= false*/) const
{
    Vec2d  direction = Vec2d::fromPoint(toCenter - m_center).truncate(iterationSize);
    double distance = m_center.getDistanceTo(toCenter);

    bool  isPathFree = true;
    Vec2d move = direction;
    do
    {
        isPathFree = !willCollide(move, obstacle, isRoughCalculation);
        move += direction;
    } while (move.length() < distance && isPathFree);

    // also check final destination:
    if (isPathFree)
        isPathFree = !willCollide(move.truncate(distance), obstacle, isRoughCalculation);

    return isPathFree;
}

bool VehicleGroupGhost::willCollide(const Vec2d& thisDisplacement, const Obstacle& other, bool isRoughCalculation /*= false*/) const
{
    Rect intersection;
    Rect actualRect = m_rect + thisDisplacement;
    if (!actualRect.overlaps(other.rect(), intersection))
        return false;

    intersection = intersection.inflate(m_original.m_maxUnitRadius + other.maxUnitRadius());

    for (size_t groupIndex = 0; groupIndex < m_unitPlaces.size(); ++groupIndex)
    {
        VehiclePtr vehicle = m_original.m_units[groupIndex].lock();
        Point actualCenter = m_unitPlaces[groupIndex] + thisDisplacement;

        if (!intersection.contains(actualCenter))
            continue;

        for (size_t otherIndex = 0; otherIndex < other.getUnitsCount(); ++otherIndex)
        {
            Point otherPoint = other.getUnitPoint(otherIndex);
            if (!intersection.contains(otherPoint))
                continue;

            if (isRoughCalculation)
                return true;

            double squaredDistance = actualCenter.getSquareDistance(otherPoint);
            double sqaredRadius = Point::pow2(vehicle->getRadius() + other.getUnitRadius(otherIndex));

            if (squaredDistance < sqaredRadius)
                return true;
        }
    }

    return false;
}

const VehicleGroupGhost Obstacle::s_nullGhost = VehicleGroupGhost(VehicleGroup(), Vec2d());
