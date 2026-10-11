#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "area.hpp"
#include "shapes.hpp"

int main(int argc, char **argv)
{
  if (argc != 3 && argc != 4) {
    std::cerr << "wrong ammount of arguments";
    return 1;
  }
  if (argv[1][0] == '-' || argv[2][0] == '-') {
    std::cerr << "Negative argument\n";
    return 1;
  }

  size_t threads = 0;
  size_t tries = 0;
  size_t seed = 0;
  try {
    threads = std::stoull(argv[1]);
    tries = std::stoull(argv[2]);
    threads = threads ? threads : 1;
    if (tries == 0) {
      std::cerr << "tries must be positive number\n";
      return 1;
    }
    if (argc == 4) {
      if (argv[3][0] == '-') {
        std::cerr << "Negative argument\n";
        return 1;
      }
      seed = std::stoull(argv[3]);
    }
  } catch (const std::invalid_argument &) {
    std::cerr << "Not a number in arguments\n";
    return 1;
  } catch (const std::out_of_range &) {
    std::cerr << "Overflow in arguments\n";
    return 1;
  }

  double r = 0, place_holder = 0, x = 0, y = 0;
  std::vector< saldaev::Circle > circles;
  while (std::cin >> r >> place_holder >> x >> y) {
    circles.push_back({r, {x, y}});
  }
  if (!std::cin.eof()) {
    std::cerr << "invalid input";
    return 1;
  }

  if (circles.size() == 0) {
    std::cout << "0.0 0.0\n";
    return 0;
  }

  const std::pair< double, double > areas = saldaev::area(circles, threads, tries, seed);

  std::cout << areas.first << ' ' << areas.second << '\n';
}
