#include <algorithm>
#include <cmath>
#include <csignal>
#include <fcntl.h>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace saldaev {
  struct Point {
    double x, y;
  };

  struct Frame {
    Point min, max;
  };

  struct Shape {
    std::pair< long long, long long > position;
    explicit Shape(std::pair< long long, long long > p):
      position{p}
    {}
    virtual bool contain(const Point p) const noexcept = 0;
    virtual void rotate(double ksi) noexcept = 0;
    virtual Frame boundingFrame() const noexcept = 0;
    virtual ~Shape() = default;
    virtual std::string toRawData() const = 0;
    virtual std::string toOutputData() const = 0;
  };

  struct Circle: Shape {
    long long radius;

    Circle(long long r, std::pair< long long, long long > p):
      Shape{p},
      radius{r}
    {}

    bool contain(const Point p) const noexcept override
    {
      const double px = position.first;
      const double py = position.second;
      return (p.x - px) * (p.x - px) + (p.y - py) * (p.y - py) <= radius * radius;
    }

    void rotate(double) noexcept override
    {}

    Frame boundingFrame() const noexcept override
    {
      const double px = position.first;
      const double py = position.second;
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

  struct Ellipse: Shape {
    long long radius_x, radius_y;
    double angle;

    Ellipse(long long rx, long long ry, std::pair< long long, long long > p, double a = 0.0):
      Shape{p},
      radius_x{rx},
      radius_y{ry},
      angle{a}
    {}

    bool contain(const Point p) const noexcept override
    {
      const double dx = p.x - position.first;
      const double dy = p.y - position.second;
      const double c = std::cos(angle);
      const double s = std::sin(angle);

      const double lx = c * dx + s * dy;
      const double ly = -s * dx + c * dy;

      const double u = lx / radius_x;
      const double v = ly / radius_y;
      return u * u + v * v <= 1.0;
    }

    void rotate(double ksi) noexcept override
    {
      angle += ksi;
    }

    Frame boundingFrame() const noexcept override
    {
      const double c = std::cos(angle);
      const double s = std::sin(angle);

      const double hw = std::sqrt((radius_x * c) * (radius_x * c) + (radius_y * s) * (radius_y * s));
      const double hh = std::sqrt((radius_x * s) * (radius_x * s) + (radius_y * c) * (radius_y * c));

      const double px = position.first;
      const double py = position.second;

      return {{px - hw, py - hh}, {px + hw, py + hh}};
    }

    std::string toRawData() const override
    {
      std::string res = "E";
      res += ' ' + std::to_string(radius_x);
      res += ' ' + std::to_string(radius_y);
      res += ' ' + std::to_string(position.first);
      res += ' ' + std::to_string(position.second);
      res += ' ' + std::to_string(angle);
      return res;
    }

    std::string toOutputData() const override
    {
      std::string res = std::to_string(radius_x);
      res += ' ' + std::to_string(radius_y);
      res += " (" + std::to_string(position.first);
      res += ' ' + std::to_string(position.second);
      res += ") " + std::to_string(angle);
      return res;
    }
  };

  enum class CalcStatus { QUEUED, IN_PROGRESS, DONE, CANCELED };

  struct Calculation {
    volatile CalcStatus status;
    double area;
    long long elapsed_ms;
  };

  struct ResultMsg {
    long long calc_id;
    double area;
    long long elapsed_ms;
  };

  using Calculations = std::map< long long, Calculation >;

  struct ProcessState {
    Calculations *calcs;
    int write_fd;
    int read_fd;
    pid_t pid;
  };

  struct CalcRef {
    long long id;
    std::string proc_name;
  };

  using Processes = std::map< std::string, ProcessState >;
  using Shapes = std::map< std::string, std::unique_ptr< Shape > >;
  using Sets = std::map< std::string, std::vector< std::string > >;
  using ShapeVector = std::vector< std::unique_ptr< Shape > >;

  struct global_data {
    Shapes shapes;
    Sets sets;
    Processes processes;
    std::map< std::string, CalcRef > calc_registry;
    long long global_calc_id = 0;
  };

  Frame getBorders(const std::vector< Shape * > &shapes)
  {
    const double lim = std::numeric_limits< double >::infinity();

    Point min{lim, lim};
    Point max{-lim, -lim};

    for (const Shape *const shape : shapes) {
      const Frame fr = shape->boundingFrame();
      min = {std::min(min.x, fr.min.x), std::min(min.y, fr.min.y)};
      max = {std::max(max.x, fr.max.x), std::max(max.y, fr.max.y)};
    }

    return {min, max};
  }

  Frame getBorders(ShapeVector &shapes)
  {
    const double lim = std::numeric_limits< double >::infinity();

    Point min{lim, lim};
    Point max{-lim, -lim};

    for (auto &shape : shapes) {
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

  size_t calculate(Frame frame, const ShapeVector &shapes, size_t tries, size_t seed)
  {
    std::default_random_engine gen(seed);
    std::uniform_real_distribution< double > width(frame.min.x, frame.max.x);
    std::uniform_real_distribution< double > hight(frame.min.y, frame.max.y);

    size_t hits = 0;

    for (size_t i = 0; i < tries; ++i) {
      const Point p{width(gen), hight(gen)};

      for (size_t j = 0; j < shapes.size(); ++j) {
        if (shapes[j]->contain(p)) {
          hits++;
          break;
        }
      }
    }

    return hits;
  }

  double area(ShapeVector &shapes, size_t threads, size_t tries, size_t seed)
  {
    std::vector< std::future< size_t > > futures;

    const size_t tries_per_thread = tries / threads;
    const size_t remainder = tries % threads;

    Frame frame = getBorders(shapes);
    for (size_t i = 0; i < threads; ++i) {
      size_t part = tries_per_thread + (i < remainder ? 1 : 0);
      futures.push_back(std::async(std::launch::async, calculate, frame, std::cref(shapes), part, seed + i));
    }

    size_t hits = 0;
    for (size_t i = 0; i < threads; i++) {
      hits += futures[i].get();
    }

    const double frame_area = (frame.max.x - frame.min.x) * (frame.max.y - frame.min.y);
    return {frame_area * (static_cast< double >(hits) / tries)};
  }

  void workerLoop(size_t seed)
  {
    long long id = 0;
    while (std::cin >> id) {
      ShapeVector shapes;
      std::string type;
      size_t n = 0;
      long long x = 0, y = 0, rx = 0, ry = 0;
      double a = 0.0;
      std::cin >> n;
      for (size_t i = 0; i < n; ++i) {
        std::cin >> type;
        if (type == "C") {
          std::cin >> rx >> x >> y;
          if (std::cin.fail()) {
            return;
          }
          shapes.push_back(std::make_unique< Circle >(rx, std::make_pair(x, y)));
        } else if (type == "E") {
          std::cin >> rx >> ry >> x >> y >> a;
          if (std::cin.fail()) {
            return;
          }
          shapes.push_back(std::make_unique< Ellipse >(rx, ry, std::make_pair(x, y), a));
        }
      }
      long long threads = 0, tries = 0;
      std::cin >> threads >> tries;
      if (std::cin.fail()) {
        return;
      }

      std::cout.write(reinterpret_cast< const char * >(&id), sizeof(long long));
      std::cout.flush();

      auto start = std::chrono::steady_clock::now();

      const double result = area(shapes, threads, tries, seed);

      auto end = std::chrono::steady_clock::now();

      ResultMsg msg;
      msg.calc_id = id;
      msg.area = result;
      msg.elapsed_ms = std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count();
      std::cout.write(reinterpret_cast< const char * >(&msg), sizeof(ResultMsg));
      std::cout.flush();
    }
  }

  void readerLoop(int fd, Calculations *calcs)
  {
    while (true) {
      long long calc_id;
      size_t total_read = 0;
      char *buf = reinterpret_cast< char * >(&calc_id);

      while (total_read < sizeof(long long)) {
        const ssize_t n = read(fd, buf + total_read, sizeof(long long) - total_read);
        if (n > 0) {
          total_read += n;
        } else if (n == 0) {
          return;
        } else {
          if (errno == EINTR) {
            continue;
          }
          return;
        }
      }

      Calculation &calc = (*calcs)[calc_id];
      calc.status = CalcStatus::IN_PROGRESS;

      ResultMsg msg;
      total_read = 0;
      buf = reinterpret_cast< char * >(&msg);

      while (total_read < sizeof(ResultMsg)) {
        const ssize_t n = read(fd, buf + total_read, sizeof(ResultMsg) - total_read);
        if (n > 0) {
          total_read += n;
        } else if (n == 0) {
          return;
        } else {
          if (errno == EINTR) {
            continue;
          }
          return;
        }
      }

      calc.area = msg.area;
      calc.elapsed_ms = msg.elapsed_ms;

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      calc.status = CalcStatus::DONE;
    }
  }

  bool sendData(int fd, const std::string &data)
  {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
      const ssize_t sent = write(fd, data.c_str() + total_sent, data.size() - total_sent);

      if (sent == -1) {
        if (errno == EINTR) {
          continue;
        }
        return false;
      }

      total_sent += sent;
    }
    return true;
  }

  void clearPipe(int fd)
  {
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    const size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    while (true) {
      const ssize_t n = read(fd, buf, sizeof(buf));
      if (n <= 0) {
        break;
      }
    }
  }

  std::vector< std::string > parse(std::string rawLine)
  {
    std::vector< std::string > tokens;
    size_t start = 0;
    size_t index = rawLine.find(' ');
    std::string token = "";
    while (index != std::string::npos) {
      token = rawLine.substr(start, index - start);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      start = index + 1;
      index = rawLine.find(' ', start);
    }
    token = rawLine.substr(start);
    if (!token.empty()) {
      tokens.push_back(token);
    }

    return tokens;
  }

  void handleCircle(std::istream &in, std::ostream &, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 4) {
      throw std::exception();
    }

    std::string name = tokens[0];
    long long r = std::stoll(tokens[1]);
    long long x = std::stoll(tokens[2]);
    long long y = std::stoll(tokens[3]);

    if (r <= 0 || global_data.shapes.find(name) != global_data.shapes.end()) {
      throw std::exception();
    }
    global_data.shapes[name] = std::make_unique< Circle >(r, std::make_pair(x, y));
  }

  void handleEllipse(std::istream &in, std::ostream &, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 5) {
      throw std::exception();
    }

    std::string name = tokens[0];
    long long rx = std::stoll(tokens[1]);
    long long ry = std::stoll(tokens[2]);
    long long x = std::stoll(tokens[3]);
    long long y = std::stoll(tokens[4]);

    if (rx <= 0 || ry <= 0 || global_data.shapes.find(name) != global_data.shapes.end()) {
      throw std::exception();
    }
    global_data.shapes[name] = std::make_unique< Ellipse >(rx, ry, std::make_pair(x, y));
  }

  void handleRotate(std::istream &in, std::ostream &, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 2) {
      throw std::exception();
    }

    std::string name = tokens[0];
    double ksi = std::stod(tokens[1]);

    auto it = global_data.shapes.find(name);
    if (it == global_data.shapes.end()) {
      throw std::exception();
    }
    it->second->rotate(ksi);
  }

  void handleSet(std::istream &in, std::ostream &, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    long long t_s = tokens.size();
    if (t_s < 2) {
      throw std::exception();
    }

    std::string name = tokens[0];
    long long n = std::stoll(tokens[1]);
    if (n < 0 || t_s != 2 + n) {
      throw std::exception();
    }
    if (global_data.sets.find(name) != global_data.sets.end()) {
      throw std::exception();
    }

    std::string shape;
    std::vector< std::string > v;
    for (long long i = 0; i < n; ++i) {
      shape = tokens[2 + i];
      if (global_data.shapes.find(shape) != global_data.shapes.end()) {
        v.push_back(shape);
      } else {
        throw std::exception();
      }
    }
    if (in.eof()) {
      return;
    }

    global_data.sets[name] = v;
  }

  void handleShow(std::istream &in, std::ostream &out, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 1) {
      throw std::exception();
    }

    std::string name = tokens[0];

    auto it = global_data.shapes.find(name);
    if (it == global_data.shapes.end()) {
      throw std::exception();
    }

    out << it->second->toOutputData() << "\n";
  }

  void handleShowset(std::istream &in, std::ostream &out, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 1) {
      throw std::exception();
    }

    std::string name = tokens[0];

    auto it = global_data.sets.find(name);
    if (it == global_data.sets.end()) {
      throw std::exception();
    }

    for (std::string str : it->second) {
      out << global_data.shapes.at(str)->toOutputData() << "\n";
    }
  }

  void handleFrame(std::istream &in, std::ostream &out, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 1) {
      throw std::exception();
    }

    std::string name = tokens[0];

    auto it = global_data.shapes.find(name);
    if (it == global_data.shapes.end()) {
      throw std::exception();
    }

    printFrame(it->second->boundingFrame(), out);
  }

  void handleFrameset(std::istream &in, std::ostream &out, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 1) {
      throw std::exception();
    }

    std::string name = tokens[0];

    auto it = global_data.sets.find(name);
    if (it == global_data.sets.end()) {
      throw std::exception();
    }

    std::vector< Shape * > v;
    for (std::string str : it->second) {
      v.push_back(global_data.shapes[str].get());
    }

    printFrame(saldaev::getBorders(v), out);
  }

  void handleSpawn(std::istream &in, std::ostream &, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 2) {
      throw std::exception();
    }

    std::string name = tokens[0];
    long long seed = std::stoll(tokens[1]);

    if (seed < 0 || global_data.processes.find(name) != global_data.processes.end()) {
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

      workerLoop(seed);
      _exit(0);
    } else if (pid > 0) {
      close(results_pipe[1]);

      Calculations *calcs = new Calculations();

      ProcessState state;
      state.calcs = calcs;
      state.write_fd = data_pipe[1];
      state.read_fd = data_pipe[0];
      state.pid = pid;

      global_data.processes[name] = state;

      std::thread(readerLoop, results_pipe[0], calcs).detach();
    } else {
      close(data_pipe[0]);
      close(data_pipe[1]);
      close(results_pipe[0]);
      close(results_pipe[1]);
      throw std::exception();
    }
  }

  void handleAreaOn(std::istream &in, std::ostream &, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 5) {
      throw std::exception();
    }

    std::string p_name = tokens[0];
    std::string c_name = tokens[1];
    std::string s_name = tokens[2];
    long long threads = std::stoll(tokens[3]);
    long long tries = std::stoll(tokens[4]);

    auto it_p = global_data.processes.find(p_name);
    auto it_s = global_data.sets.find(s_name);
    if (it_p == global_data.processes.end() || it_s == global_data.sets.end() || threads <= 0 || tries <= 0) {
      throw std::exception();
    }
    if (global_data.calc_registry.find(c_name) != global_data.calc_registry.end()) {
      throw std::exception();
    }

    long long id = global_data.global_calc_id++;
    global_data.calc_registry[c_name] = {id, p_name};
    (*it_p->second.calcs)[id] = {CalcStatus::QUEUED, 0.0, 0};

    int fd = it_p->second.write_fd;

    std::string payload;
    payload += std::to_string(id) + ' ' + std::to_string(it_s->second.size()) + '\n';

    for (const std::string &shape_name : it_s->second) {
      payload += global_data.shapes.at(shape_name)->toRawData() + '\n';
    }

    payload += std::to_string(threads) + ' ' + std::to_string(tries) + '\n';

    if (!sendData(fd, payload)) {
      (*it_p->second.calcs)[id].status = CalcStatus::CANCELED;
      throw std::exception();
    }
  }

  void handleStatus(std::istream &in, std::ostream &out, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 1) {
      throw std::exception();
    }

    std::string name = tokens[0];

    if (global_data.calc_registry.find(name) == global_data.calc_registry.end()) {
      throw std::exception();
    }

    size_t id = global_data.calc_registry.at(name).id;
    std::string proc = global_data.calc_registry.at(name).proc_name;
    Calculation &calc = global_data.processes.at(proc).calcs->at(id);

    if (calc.status == CalcStatus::IN_PROGRESS) {
      out << proc << " <IN PROGRESS>\n";
    } else if (calc.status == CalcStatus::DONE) {
      out << proc << ' ' << calc.elapsed_ms << ' ' << calc.area << '\n';
    } else {
      throw std::exception();
    }
  }

  void handleWait(std::istream &in, std::ostream &out, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 1) {
      throw std::exception();
    }

    std::string name = tokens[0];

    if (global_data.calc_registry.find(name) == global_data.calc_registry.end()) {
      throw std::exception();
    }

    size_t id = global_data.calc_registry.at(name).id;
    std::string proc = global_data.calc_registry.at(name).proc_name;
    Calculation &calc = (*global_data.processes.at(proc).calcs)[id];

    if (calc.status == CalcStatus::QUEUED) {
      throw std::exception();
    }

    while (calc.status != CalcStatus::DONE) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    out << proc << ' ' << calc.elapsed_ms << ' ' << calc.area << '\n';
  }

  void handleTimeout(std::istream &in, std::ostream &out, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 2) {
      throw std::exception();
    }

    std::string name = tokens[0];
    long long waitTime = std::stoll(tokens[1]);

    if (waitTime <= 0 || global_data.calc_registry.find(name) == global_data.calc_registry.end()) {
      throw std::exception();
    }

    size_t id = global_data.calc_registry.at(name).id;
    std::string proc = global_data.calc_registry.at(name).proc_name;
    Calculation &calc = global_data.processes.at(proc).calcs->at(id);

    if (calc.status == CalcStatus::QUEUED) {
      throw std::exception();
    }

    size_t timePassed = 0;
    while (calc.status != CalcStatus::DONE && timePassed < waitTime) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      timePassed += 10;
    }
    if (calc.status != CalcStatus::DONE) {
      out << proc << " <IN PROGRESS>\n";
    }
    out << proc << ' ' << calc.elapsed_ms << ' ' << calc.area << '\n';
  }

  void handleKill(std::istream &in, std::ostream &, global_data &global_data)
  {
    std::string line;
    std::getline(in, line);
    std::vector< std::string > tokens = parse(line);
    if (tokens.size() != 1) {
      throw std::exception();
    }

    std::string name = tokens[0];

    if (global_data.processes.find(name) == global_data.processes.end()) {
      throw std::exception();
    }

    const ProcessState &proc = global_data.processes.at(name);

    clearPipe(proc.read_fd);

    const char *kill_msg = "die pls\n";
    const size_t KILL_MSG_LEN = 7;
    write(proc.write_fd, kill_msg, KILL_MSG_LEN);

    for (auto &pair : *(proc.calcs)) {
      if (pair.second.status == CalcStatus::QUEUED) {
        pair.second.status = CalcStatus::CANCELED;
      }
    }

    close(proc.read_fd);
    close(proc.write_fd);
  }
}

int main()
{
  std::signal(SIGPIPE, SIG_IGN);
  using Command = void (*)(std::istream &, std::ostream &, saldaev::global_data &);
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

  saldaev::global_data global_data;

  std::string cmd = "";
  while (std::cin >> cmd) {
    auto it = commands.find(cmd);
    if (it != commands.end()) {
      try {
        it->second(std::cin, std::cout, global_data);
      } catch (const std::exception &) {
        std::cout << "<INVALID COMMAND>\n";
      }
    } else {
      std::cout << "<UNKNOWN COMMAND>\n";
      std::cin.clear();
      std::cin.ignore(std::numeric_limits< std::streamsize >::max(), '\n');
    }
  }
}
