#include "prx/libSceAgcDriver/Graphics/include/UnitFlags.hpp"
#include <algorithm>
#include <barrier>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>
#include <thread>
using namespace AgcDriver::Graphics;
namespace {

template <class TFlags> void CheckModel() {
  const auto check = [](bool value) {
    if (!value)
      throw std::runtime_error("unit flag reference model mismatch");
  };
  std::mt19937_64 random(0x9123);
  for (std::size_t size :
       {0u, 1u, 63u, 64u, 65u, 127u, 128u, 129u, 510u, 4097u}) {
    TFlags flags;
    for (bool initial : {false, true}) {
      flags.assign(size, initial);
      std::vector<bool> model(size, initial);
      for (unsigned step = 0; step < 2000; ++step) {
        auto first = size == 0 ? 0 : random() % (size + 1);
        auto length = random() % (size - first + 1);
        auto value = (random() & 1) != 0;
        if (size && (random() & 1)) {
          auto index = random() % size;
          flags[index] = value;
          model[index] = value;
        } else {
          flags.SetRange(first, length, value);
          std::fill(model.begin() + first, model.begin() + first + length,
                    value);
        }
        check(flags.Bools() == model);
        check(flags.Count() == static_cast<std::size_t>(std::count(
                                   model.begin(), model.end(), true)));
        check(flags.Any() ==
              (std::find(model.begin(), model.end(), true) != model.end()));
        check(flags.AnyIn(first, length) ==
              (std::find(model.begin() + first, model.begin() + first + length,
                         true) != model.begin() + first + length));
        check(flags.AllIn(first, length) ==
              (std::find(model.begin() + first, model.begin() + first + length,
                         false) == model.begin() + first + length));
        const TFlags &read = flags;
        if (size)
          check(read[first % size] == model[first % size]);
        check(flags.size() == size && flags.empty() == (size == 0));
      }
    }
    flags.clear();
    check(flags.empty() && flags.size() == 0 && !flags.Any());
  }
}
void CheckConcurrent() {
  bool valid = true;
  const auto check = [&](bool value) { valid = valid && value; };
  PendingUnitFlags flags;
  flags.assign(129, false);
  std::barrier phase(9);
  std::vector<std::thread> workers;
  for (unsigned bit = 0; bit < 8; ++bit)
    workers.emplace_back([&, bit] {
      for (unsigned pass = 0; pass < 10000; ++pass) {
        phase.arrive_and_wait();
        flags[bit] = true;
        flags.SetRange(64 + bit, 1, true);
        phase.arrive_and_wait();
        phase.arrive_and_wait();
        flags[bit] = false;
        flags.SetRange(64 + bit, 1, false);
        phase.arrive_and_wait();
      }
    });
  for (unsigned pass = 0; pass < 10000; ++pass) {
    phase.arrive_and_wait();
    phase.arrive_and_wait();
    check(flags.Count() == 16 && flags.AllIn(0, 8) && flags.AllIn(64, 8));
    phase.arrive_and_wait();
    phase.arrive_and_wait();
    check(!flags.Any());
  }
  for (auto &t : workers)
    t.join();
  std::atomic<bool> done = false;
  std::thread reset([&] {
    for (unsigned i = 0; i < 100000; ++i)
      flags.assign(129, (i & 1) != 0);
    done = true;
  });
  while (!done.load()) {
    check(flags.size() == 129);
    check(flags.Count() <= 129);
    flags.AnyIn(60, 69);
    flags.AllIn(60, 69);
    flags.Bools();
  }
  reset.join();
  if (!valid)
    throw std::runtime_error(
        "concurrent unit flags lost bits or changed their shape");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("expected model or concurrent case");
    const std::string_view mode(argv[1]);
    if (mode == "model") {
      CheckModel<UnitFlags>();
      CheckModel<PendingUnitFlags>();
    } else if (mode == "concurrent") {
      CheckConcurrent();
    } else {
      throw std::runtime_error("unknown unit flags case");
    }
    std::cout << "unit flags " << mode << " checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
