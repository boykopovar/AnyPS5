#include "prx/libc/include/HostThreadLocal.hpp"
#include <atomic>
#include <cstdlib>
#include <vector>

namespace {
std::atomic<unsigned> destroyed{0};
struct Value {
    std::vector<unsigned> data = std::vector<unsigned>(256, 42);
    ~Value() {
        for (auto item : data) if (item != 42) std::abort();
        ++destroyed;
    }
};
struct First {};
struct Second {};
struct Unused {};
}

#ifdef _WIN32
#define TEST_EXPORT __declspec(dllexport)
#else
#define TEST_EXPORT
#endif

extern "C" TEST_EXPORT void TouchHostThreadLocal() {
#ifdef _WIN32
    if (FindHostThreadLocal<Value, Unused>() != nullptr) std::abort();
#endif
    auto& first = HostThreadLocal<Value, First>();
    auto& second = HostThreadLocal<Value, Second>();
    if (&first == &second || &first != &HostThreadLocal<Value, First>()) std::abort();
#ifdef _WIN32
    if (FindHostThreadLocal<Value, First>() != &first || FindHostThreadLocal<Value, Second>() != &second) std::abort();
#endif
}

extern "C" TEST_EXPORT unsigned DestroyedHostThreadLocals() { return destroyed.load(); }
