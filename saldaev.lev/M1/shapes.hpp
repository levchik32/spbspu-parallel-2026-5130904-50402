#ifndef SHAPES_HPP
#define SHAPES_HPP

namespace saldaev
{
  struct Point
  {
    double x, y;
  };

  struct Circle
  {
    double radius;
    Point position;
    bool contain(Point point) const noexcept;
  };

  struct Frame
  {
    Point min, max;
  };
};

#endif
