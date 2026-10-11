#include "shapes.hpp"

bool saldaev::Circle::contain(Point p) const noexcept
{
  return (p.x - position.x) * (p.x - position.x) + (p.y - position.y) * (p.y - position.y) <= radius * radius;
}
