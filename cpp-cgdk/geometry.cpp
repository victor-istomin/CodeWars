#include "geometry.h"

const double Point::k_epsilon = 0.0001;   // TODO - get epsilon from game system!

bool Rect::overlaps(const Rect& other) const
{
	bool noOverlap = this->m_topLeft.m_x > other.m_bottomRight.m_x || other.m_topLeft.m_x > this->m_bottomRight.m_x
	              || this->m_topLeft.m_y > other.m_bottomRight.m_y || other.m_topLeft.m_y > this->m_bottomRight.m_y;

	return !noOverlap;
}

bool Rect::overlaps(const Rect& other, Rect& intersection) const
{
	bool doesOverlap = overlaps(other);
	if (doesOverlap)
		intersection = Rect({ std::max(this->m_topLeft.m_x,     other.m_topLeft.m_x),     std::max(this->m_topLeft.m_y,     other.m_topLeft.m_y) },
	                        { std::min(this->m_bottomRight.m_x, other.m_bottomRight.m_x), std::min(this->m_bottomRight.m_y, other.m_bottomRight.m_y) });
	return doesOverlap;
}
