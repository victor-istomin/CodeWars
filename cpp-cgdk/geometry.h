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

    Point()                                                       : m_x(0), m_y(0) {}
    Point(double x, double y)                                     : m_x(x), m_y(y) {}
    Point(const model::Unit& unit)                                : m_x(unit.getX()), m_y(unit.getY()) {}

    Point& operator+=(const Point& right)                         { m_x += right.m_x; m_y += right.m_y; return *this; }
    Point& operator+=(const Vec2d& right)                         { return (*this += right.toPoint<Point>()); }
    Point& operator-=(const Point& right)                         { m_x -= right.m_x; m_y -= right.m_y; return *this; }
    Point& operator/=(double divider)                             { m_x /= divider;   m_y /= divider;   return *this; }
    Point& operator*=(double multiplier)                          { m_x *= multiplier; m_y *= multiplier; return *this; }

    friend Point operator+(const Point& left, const Point& right) { return Point(left) += right; }
    friend Point operator-(const Point& left, const Point& right) { return Point(left) -= right; }
    friend Point operator+(const Point& left, const Vec2d& right) { return Point(left) += right.toPoint<Point>(); }
    friend Point operator-(const Point& left, const Vec2d& right) { return Point(left) -= right.toPoint<Point>(); }
    friend Point operator*(const Point& left, double right)       { return Point(left) *= right; }
    friend Point operator/(const Point& left, double right)       { return Point(left) /= right; }

    bool operator==(const Point& right) const                     { return std::abs(m_x - right.m_x) < k_epsilon && std::abs(m_y - right.m_y) < k_epsilon; }
    bool operator!=(const Point& right) const                     { return !(*this == right); }

    bool isWithinRadius(double r) const                           { return pow2(m_x) + pow2(m_y) < pow2(r); }

    double getDistanceTo(const Point& other)     const            { return std::sqrt(getSquareDistance(other)); }
    double getSquareDistance(const Point& other) const            { return pow2(m_x - other.m_x) + pow2(m_y - other.m_y); }
};

struct Rect
{
    Point m_topLeft;
    Point m_bottomRight;

    Rect()                                                 : m_topLeft(), m_bottomRight() {}

    Rect(const Point& topLeft, const Point& bottomRight)   : m_topLeft(topLeft), m_bottomRight(bottomRight) {}

    Point bottomLeft() const                               { return Point(m_topLeft.m_x, m_bottomRight.m_y); }
    Point topRight()   const                               { return Point(m_bottomRight.m_x, m_topLeft.m_y); }

    // check if rect overlaps with other one
    bool overlaps(const Rect& other) const;
    bool overlaps(const Rect& other, Rect& intersection) const;
    bool overlapsCircle(const Point& center, double radius) const;

    bool contains(const Point& point) const;

    double height() const                                  { return std::abs(m_bottomRight.m_y - m_topLeft.m_y); }
    double width()  const                                  { return std::abs(m_bottomRight.m_x - m_topLeft.m_x); }

    // ensure 'inside' point is actually inside rect 
    void ensureContains(const Point& inside);
    void ensureContains(const Rect& inside);

    Rect inflate(const Point& dxdy) const                  { return Rect(m_topLeft - dxdy, m_bottomRight + dxdy); }
    Rect inflate(double amount) const                      { return inflate(Point(amount, amount)); }

    Rect& operator+=(const Vec2d& v)                       { m_topLeft += v.toPoint<Point>(); m_bottomRight += v.toPoint<Point>(); return *this; }
    friend Rect operator+(Rect rect, const Vec2d& v)       { return rect += v; }

    bool operator==(const Rect& right) const               { return m_topLeft == right.m_topLeft && m_bottomRight == right.m_bottomRight; }
};

