#include <algorithm>
#include <cmath>
#include <csignal>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
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
    std::pair< long long, long long > position;
    explicit Shape(std::pair< long long, long long > p):
      position{p}
    {}
    virtual bool contain(Point p) const noexcept = 0;
    virtual void rotate(double ksi) noexcept = 0;
    virtual Frame boundingFrame() const noexcept = 0;
    virtual ~Shape() = default;
    virtual std::string toRawData() const = 0;
    virtual std::string toOutputData() const = 0;
  };

  struct Circle: Shape
  {
    long long radius;

    Circle(long long r, std::pair< long long, long long > p):
      Shape{p},
      radius{r}
    {}

    bool contain(Point p) const noexcept override
    {
      double px = position.first;
      double py = position.second;
      return (p.x - px) * (p.x - px) + (p.y - py) * (p.y - py) <= radius * radius;
    }

    void rotate(double ksi) noexcept override
    {}

    Frame boundingFrame() const noexcept override
    {
      double px = position.first;
      double py = position.second;
      return {{px - radius, py - radius}, {px + radius, py + radius}};
    }

    std::string toRawData() const override
    {
      std::string res = "C";
      res += ' ' + std::to_string(radius);
      res += ' ' + std::to_string(position.first);
      res += ' ' + std::to_string(position.second);
      return res;
    }

    std::string toOutputData() const override
    {
      std::string res = std::to_string(radius);
      res += " (" + std::to_string(position.first);
      res += ' ' + std::to_string(position.second) + ')';
      return res;
    }
  };

  struct Ellipse: Shape
  {
    long long radiusX, radiusY;
    double angle;

    Ellipse(long long rx, long long ry, std::pair< long long, long long > p, double a = 0.0):
      Shape{p},
      radiusX{rx},
      radiusY{ry},
      angle{a}
    {}

    bool contain(Point p) const noexcept override
    {
      double dx = p.x - position.first;
      double dy = p.y - position.second;
      double c = std::cos(angle);
      double s = std::sin(angle);

      double lx = c * dx + s * dy;
      double ly = -s * dx + c * dy;

      double u = lx / radiusX;
      double v = ly / radiusY;
      return u * u + v * v <= 1.0;
    }

    void rotate(double ksi) noexcept override
    {
      angle += ksi;
    }

    Frame boundingFrame() const noexcept override
    {
      double c = std::cos(angle);
      double s = std::sin(angle);

      double hw = std::sqrt((radiusX * c) * (radiusX * c) + (radiusY * s) * (radiusY * s));
      double hh = std::sqrt((radiusX * s) * (radiusX * s) + (radiusY * c) * (radiusY * c));

      double px = position.first;
      double py = position.second;

      return {{px - hw, py - hh}, {px + hw, py + hh}};
    }

    std::string toRawData() const override
    {
      std::string res = "E";
      res += ' ' + std::to_string(radiusX);
      res += ' ' + std::to_string(radiusY);
      res += ' ' + std::to_string(position.first);
      res += ' ' + std::to_string(position.second);
      res += ' ' + std::to_string(angle);
      return res;
    }

    std::string toOutputData() const override
    {
      std::string res = std::to_string(radiusX);
      res += ' ' + std::to_string(radiusY);
      res += " (" + std::to_string(position.first);
      res += ' ' + std::to_string(position.second);
      res += ") " + std::to_string(angle);
      return res;
    }
  };

  enum class CalcStatus { QUEUED, IN_PROGRESS, DONE, CANCELED };

  struct Calculation
  {
    volatile CalcStatus status;
    double area;
    double intersection;
    long long elapsed_ms;
  };

  struct ResultMsg
  {
    long long calc_id;
    double area;
    double intersection;
    long long elapsed_ms;
  };

  using Calculations = std::map< long long, Calculation >;
  using Processes = std::map< std::string, std::pair< Calculations *, int > >;
  using Name_to_id = std::map< std::string, long long >;

  Frame getBorders(const std::vector< Shape * > &shapes)
  {
    const double lim = std::numeric_limits< double >::infinity();

    Point min{lim, lim};
    Point max{-lim, -lim};

    for (Shape *shape : shapes) {
      Frame fr = shape->boundingFrame();
      min = {std::min(min.x, fr.min.x), std::min(min.y, fr.min.y)};
      max = {std::max(max.x, fr.max.x), std::max(max.y, fr.max.y)};
    }

    return {min, max};
  }

  void printFrame(const Frame &fr, std::ostream &out)
  {
    out << "(" << static_cast< long long >(fr.min.x) << " " << static_cast< long long >(fr.min.y) << ") "
        << "(" << static_cast< long long >(fr.max.x) << " " << static_cast< long long >(fr.max.y) << ")\n";
  }

  using Shapes = std::map< std::string, std::unique_ptr< Shape > >;
  using Sets = std::map< std::string, std::vector< std::string > >;

  struct GlobalData
  {
    Shapes shapes;
    Sets sets;
    Processes processes;
    std::map< std::string, long long > name_to_id;
    long long global_calc_id = 0;
  };

  void handleCircle(std::istream &, std::ostream &, GlobalData &);
  void handleEllipse(std::istream &, std::ostream &, GlobalData &);
  void handleRotate(std::istream &, std::ostream &, GlobalData &);
  void handleSet(std::istream &, std::ostream &, GlobalData &);
  void handleShow(std::istream &, std::ostream &, GlobalData &);
  void handleShowset(std::istream &, std::ostream &, GlobalData &);
  void handleFrame(std::istream &, std::ostream &, GlobalData &);
  void handleFrameset(std::istream &, std::ostream &, GlobalData &);
  void handleSpawn(std::istream &, std::ostream &, GlobalData &);
  void handleAreaOn(std::istream &, std::ostream &, GlobalData &);
  void handleStatus(std::istream &, std::ostream &, GlobalData &);
  void handleWait(std::istream &, std::ostream &, GlobalData &);
  void handleTimeout(std::istream &, std::ostream &, GlobalData &);
  void handleKill(std::istream &, std::ostream &, GlobalData &);

  void beCalculator();
  void readerLoop(int fd, Calculations *calcs);
}

int main()
{
  std::signal(SIGPIPE, SIG_IGN);
  using Command = void (*)(std::istream &, std::ostream &, saldaev::GlobalData &);
  using Commands = std::map< std::string, Command >;

  Commands commands;
  commands["circle"] = saldaev::handleCircle;
  commands["ellipse"] = saldaev::handleEllipse;
  commands["rotate"] = saldaev::handleRotate;
  commands["set"] = saldaev::handleSet;
  commands["show"] = saldaev::handleShow;
  commands["showset"] = saldaev::handleShowset;
  commands["frame"] = saldaev::handleFrame;
  commands["frameset"] = saldaev::handleFrameset;
  commands["spawn"] = saldaev::handleSpawn;
  commands["area-on"] = saldaev::handleAreaOn;
  commands["status"] = saldaev::handleStatus;
  commands["wait"] = saldaev::handleWait;
  commands["timeout"] = saldaev::handleTimeout;
  commands["kill"] = saldaev::handleKill;

  saldaev::GlobalData globalData;

  std::string cmd = "";
  while (std::cin >> cmd) {

    auto it = commands.find(cmd);
    if (it != commands.end()) {
      try {
        it->second(std::cin, std::cout, globalData);
      } catch (const std::exception &) {
        std::cout << "<INVALID COMMAND>\n";
      }
    } else {
      std::cout << "<UNKNOWN COMMAND>\n";
    }
    std::cin.clear();
    std::cin.ignore(std::numeric_limits< std::streamsize >::max(), '\n');
  }
}

void saldaev::handleCircle(std::istream &in, std::ostream &, GlobalData &globalData)
{
  std::string name;
  long long r, x, y;
  in >> name >> r >> x >> y;
  if (in.eof()) {
    return;
  }
  if (in.fail() || r <= 0 || globalData.shapes.find(name) != globalData.shapes.end()) {
    throw std::exception();
  }
  globalData.shapes[name] = std::make_unique< Circle >(r, std::make_pair(x, y));
}

void saldaev::handleEllipse(std::istream &in, std::ostream &, GlobalData &globalData)
{
  std::string name;
  long long rx, ry, x, y;
  in >> name >> rx >> ry >> x >> y;
  if (in.eof()) {
    return;
  }
  if (in.fail() || rx <= 0 || ry <= 0 || globalData.shapes.find(name) != globalData.shapes.end()) {
    throw std::exception();
  }
  globalData.shapes[name] = std::make_unique< Ellipse >(rx, ry, std::make_pair(x, y));
}

void saldaev::handleRotate(std::istream &in, std::ostream &, GlobalData &globalData)
{
  std::string name;
  double ksi;
  in >> name >> ksi;
  if (in.eof()) {
    return;
  }
  auto it = globalData.shapes.find(name);
  if (in.fail() || it == globalData.shapes.end()) {
    throw std::exception();
  }
  it->second->rotate(ksi);
}

void saldaev::handleSet(std::istream &in, std::ostream &, GlobalData &globalData)
{
  std::string name;
  size_t n;
  in >> name >> n;
  if (in.eof()) {
    return;
  }
  if (in.fail() || globalData.sets.find(name) != globalData.sets.end()) {
    throw std::exception();
  }
  std::string shape;
  std::vector< std::string > v;
  for (size_t i = 0; i < n; ++i) {
    in >> shape;
    if (globalData.shapes.find(shape) != globalData.shapes.end()) {
      v.push_back(shape);
    } else {
      throw std::exception();
    }
  }
  if (in.eof()) {
    return;
  }

  globalData.sets[name] = v;
}

void saldaev::handleShow(std::istream &in, std::ostream &out, GlobalData &globalData)
{
  std::string name;
  in >> name;
  if (in.eof()) {
    return;
  }
  auto it = globalData.shapes.find(name);
  if (in.fail() || it == globalData.shapes.end()) {
    throw std::exception();
  }

  out << it->second->toOutputData() << "\n";
}

void saldaev::handleShowset(std::istream &in, std::ostream &out, GlobalData &globalData)
{
  std::string name;
  in >> name;
  if (in.eof()) {
    return;
  }
  auto it = globalData.sets.find(name);
  if (in.fail() || it == globalData.sets.end()) {
    throw std::exception();
  }

  for (std::string str : it->second) {
    out << globalData.shapes.at(str)->toOutputData() << "\n";
  }
}

void saldaev::handleFrame(std::istream &in, std::ostream &out, GlobalData &globalData)
{
  std::string name;
  in >> name;
  if (in.eof()) {
    return;
  }
  auto it = globalData.shapes.find(name);
  if (in.fail() || it == globalData.shapes.end()) {
    throw std::exception();
  }

  saldaev::printFrame(it->second->boundingFrame(), out);
}

void saldaev::handleFrameset(std::istream &in, std::ostream &out, GlobalData &globalData)
{
  std::string name;
  in >> name;
  if (in.eof()) {
    return;
  }
  auto it = globalData.sets.find(name);
  if (in.fail() || it == globalData.sets.end()) {
    throw std::exception();
  }

  std::vector< Shape * > v;
  for (std::string str : it->second) {
    v.push_back(globalData.shapes[str].get());
  }

  saldaev::printFrame(saldaev::getBorders(v), out);
}

void saldaev::handleSpawn(std::istream &in, std::ostream &, GlobalData &globalData)
{
  std::string name;
  size_t seed;
  in >> name >> seed;
  if (in.eof()) {
    return;
  }
  if (in.fail() || globalData.processes.find(name) != globalData.processes.end()) {
    throw std::exception();
  }

  int data_pipe[2];
  int results_pipe[2];

  if (pipe(data_pipe) == -1 || pipe(results_pipe) == -1) {
    throw std::exception();
  }

  pid_t pid = fork();

  if (!pid) {
    if (close(data_pipe[1]) == -1 || close(results_pipe[0]) == -1) {
      _exit(1);
    }
    if (dup2(data_pipe[0], STDIN_FILENO) == -1 || dup2(results_pipe[1], STDOUT_FILENO) == -1) {
      _exit(1);
    }
    if (close(data_pipe[0]) == -1 || close(results_pipe[1]) == -1) {
      _exit(1);
    }

    beCalculator();
    _exit(0);
  } else if (pid > 0) {
    close(data_pipe[0]);
    close(results_pipe[1]);

    Calculations *calcs = new Calculations();
    globalData.processes[name] = std::make_pair(calcs, data_pipe[1]);

    std::thread(readerLoop, results_pipe[0], calcs).detach();
  } else {
    close(data_pipe[0]);
    close(data_pipe[1]);
    close(results_pipe[0]);
    close(results_pipe[1]);
    throw std::exception();
  }
}
