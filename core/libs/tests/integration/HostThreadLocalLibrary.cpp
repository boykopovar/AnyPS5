#include "prx/libc/include/HostThreadLocal.hpp"

#include <atomic>
#include <vector>

namespace {

std::atomic<unsigned> destroyed{0};
std::atomic<unsigned> violations{0};

struct Value {
    std::vector<unsigned> data = std::vector<unsigned>(256, 42);

    ~Value() {
        for (auto item : data) {
            if (item != 42) ++violations;
        }
        ++destroyed;
    }
};

struct First {};
struct Second {};

} // namespace

extern "C" void TouchHostThreadLocal() {
    auto& first = HostThreadLocal<Value, First>();
    auto& second = HostThreadLocal<Value, Second>();
    if (&first == &second || &first != &HostThreadLocal<Value, First>()) ++violations;
}

extern "C" unsigned DestroyedHostThreadLocals() { return destroyed.load(); }

extern "C" unsigned HostThreadLocalViolations() { return violations.load(); }
