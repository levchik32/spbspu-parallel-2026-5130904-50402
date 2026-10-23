#include <string>
#include <vector>

namespace saldaev
{
  struct Point
  {
    double x, y;
  };

  struct Frame
  {
    Point min, max;
  };

  struct Shape
  {
    Point position;
    virtual bool contain(Point p) const noexcept = 0;
    virtual void rotate(double ksi) noexcept = 0;
    virtual Frame boundingFrame() const noexcept = 0;
    virtual ~Shape() = default;
    virtual std::string toRawData() const = 0;
    virtual std::string toOutputData() const = 0;
  };

  struct Circle: Shape
  {
    double radius;
    bool contain(Point p) const noexcept override;
    void rotate(double ksi) noexcept override
    {}
    Frame boundingFrame() const noexcept override;
    std::string toRawData() const override;
    std::string toOutputData() const override;
  };

  struct Ellipse: Shape
  {
    double rx, ry;
    double angle = 0.0;
    bool contain(Point p) const noexcept override;
    void rotate(double ksi) noexcept override;
    Frame boundingFrame() const noexcept override;
    std::string toRawData() const override;
    std::string toOutputData() const override;
  };

  Frame getBorders(const std::vector< Shape * > &circles);
}

int main()
{}
