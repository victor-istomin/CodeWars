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
    {
        intersection = Rect({ std::max(this->m_topLeft.m_x,     other.m_topLeft.m_x),     std::max(this->m_topLeft.m_y,     other.m_topLeft.m_y) },
                            { std::min(this->m_bottomRight.m_x, other.m_bottomRight.m_x), std::min(this->m_bottomRight.m_y, other.m_bottomRight.m_y) });
    }

    return doesOverlap;
}

bool Rect::contains(const Point& point) const
{
	return m_topLeft.m_x <= point.m_x     && m_topLeft.m_y <= point.m_y
	    && m_bottomRight.m_x >= point.m_x && m_bottomRight.m_y >= point.m_y;
}

void Rect::ensureContains(const Point& inside)
{
    m_topLeft     = Point(std::min(m_topLeft.m_x, inside.m_x),     std::min(m_topLeft.m_y, inside.m_y));
    m_bottomRight = Point(std::max(m_bottomRight.m_x, inside.m_x), std::max(m_bottomRight.m_y, inside.m_y));
}

void Rect::ensureContains(const Rect& inside)
{
    ensureContains(inside.m_topLeft);
    ensureContains(inside.topRight());
    ensureContains(inside.m_bottomRight);
    ensureContains(inside.bottomLeft());
}
