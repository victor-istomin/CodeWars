#pragma once
#include <cmath>
#include <algorithm>

#include "model/Unit.h"
#include "Vec.h"

#undef max
#undef min

struct Point 
{ 
    static const double k_epsilon;
    static double pow2(double d) { return d*d; }

    double m_x;
    double m_y;

    Point(double x = 0, double y = 0)       : m_x(x), m_y(y)                     {}
    Point(const model::Unit& unit)          : m_x(unit.getX()), m_y(unit.getY()) {}

    Point& operator+=(const Point& right)   { m_x += right.m_x; m_y += right.m_y; return *this; }
    Point& operator-=(const Point& right)   { m_x -= right.m_x; m_y -= right.m_y; return *this; }
    Point& operator/=(double divider)       { m_x /= divider;   m_y /= divider;   return *this; }
    Point& operator*=(double multiplier)    { m_x *= multiplier;m_y *= multiplier;return *this; }
    
    friend Point operator+(const Point& left, const Point& right)    { return Point(left) += right; }
    friend Point operator-(const Point& left, const Point& right)    { return Point(left) -= right; }
	friend Point operator+(const Point& left, const Vec2d& right)    { return Point(left) += right.toPoint<Point>(); }
	friend Point operator-(const Point& left, const Vec2d& right)    { return Point(left) -= right.toPoint<Point>(); }
	friend Point operator*(const Point& left, double right)          { return Point(left) *= right; }
	friend Point operator/(const Point& left, double right)          { return Point(left) /= right; }

    bool operator==(const Point& right) const { return std::abs(m_x - right.m_x) < k_epsilon && std::abs(m_y - right.m_y) < k_epsilon; }

    double getDistanceTo(const Point& other)     const { return std::sqrt(getSquareDistance(other)); }
    double getSquareDistance(const Point& other) const { return pow2(m_x - other.m_x) + pow2(m_y - other.m_y); }  // sometimes we could compare Distance� with Radius� to omit expensive sqrt() and/or more expensive hypot()
};

struct Rect
{
	Point m_topLeft;
	Point m_bottomRight;

	Rect(const Point& topLeft = Point(), const Point& bottomRight = Point()) 
		: m_topLeft(topLeft), m_bottomRight(bottomRight) {}

	// check if rect overlaps with other one
	bool overlaps(const Rect& other) const;
	bool overlaps(const Rect& other, Rect& intersection) const;

	double height() const { return std::abs(m_bottomRight.m_y - m_topLeft.m_y); }
	double width()  const { return std::abs(m_bottomRight.m_x - m_topLeft.m_x); }

	// ensure 'inside' point is actually inside rect 
	void ensureContains(const Point& inside)
	{
		m_topLeft     = Point(std::min(m_topLeft.m_x, inside.m_x), std::min(m_topLeft.m_y, inside.m_y));
		m_bottomRight = Point(std::max(m_bottomRight.m_x, inside.m_x), std::max(m_bottomRight.m_y, inside.m_y));
	}

    Rect& operator+=(const Vec2d& v)                 { m_topLeft += v.toPoint<Point>(); m_bottomRight += v.toPoint<Point>(); return *this; }
    friend Rect operator+(Rect rect, const Vec2d& v) { return rect += v; }
};

