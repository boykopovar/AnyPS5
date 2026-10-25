#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

extern "C" {
int APS5_VABI pthread_attr_init_nid_postfix(void** attr);
int APS5_VABI pthread_attr_destroy_nid_postfix(void** attr);
int APS5_VABI pthread_attr_setsolosched_np_nid_postfix(void** attr, int solosched);
int APS5_VABI pthread_attr_getstackaddr_nid_postfix(void* const* attr, void** stackAddress);
int APS5_VABI scePthreadAttrSetaffinity(void** attr, KernelCpumask mask);
int APS5_VABI scePthreadAttrGetaffinity(void* const* attr, KernelCpumask* mask);
int APS5_VABI pthread_attr_getsolosched_np_nid_postfix(void* const* attr, int* solosched);
int APS5_VABI pthread_attr_setstackaddr_nid_postfix(void** attr, void* stackAddress);
int APS5_VABI pthread_attr_getaffinity_np_nid_postfix(void* const* attr, std::size_t cpusetSize, void* cpuset);
int APS5_VABI pthread_attr_setaffinity_np_nid_postfix(void** attr, std::size_t cpusetSize, const void* cpuset);
}

namespace {

constexpr int Efault = 14;
constexpr int Einval = 22;
constexpr int Erange = 34;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

struct Attr {
    void* handle = nullptr;
    Attr() { Require(pthread_attr_init_nid_postfix(&handle) == 0 && handle != nullptr); }
    ~Attr() { pthread_attr_destroy_nid_postfix(&handle); }
};

void Solosched() {
    Attr attr;
    int solosched = -1;
    Require(pthread_attr_getsolosched_np_nid_postfix(&attr.handle, &solosched) == 0 && solosched == 0);
    Require(pthread_attr_setsolosched_np_nid_postfix(&attr.handle, 16) == 0);
    Require(pthread_attr_getsolosched_np_nid_postfix(&attr.handle, &solosched) == 0 && solosched == 16);
    Require(pthread_attr_getsolosched_np_nid_postfix(&attr.handle, nullptr) == Einval);
    Require(pthread_attr_getsolosched_np_nid_postfix(nullptr, &solosched) == Einval);
    void* empty = nullptr;
    Require(pthread_attr_getsolosched_np_nid_postfix(&empty, &solosched) == Einval);
}

void Stackaddr() {
    Attr attr;
    alignas(16) static std::array<std::byte, 65536> stack{};
    void* stored = nullptr;
    Require(pthread_attr_setstackaddr_nid_postfix(&attr.handle, stack.data()) == 0);
    Require(pthread_attr_getstackaddr_nid_postfix(&attr.handle, &stored) == 0 && stored == stack.data());
    Require(pthread_attr_setstackaddr_nid_postfix(nullptr, stack.data()) == Einval);
    void* empty = nullptr;
    Require(pthread_attr_setstackaddr_nid_postfix(&empty, stack.data()) == Einval);
}

void GetAffinity() {
    Attr attr;
    Require(scePthreadAttrSetaffinity(&attr.handle, 0x5u) == 0);
    std::array<unsigned char, 40> set{};
    set.fill(0xccu);
    Require(pthread_attr_getaffinity_np_nid_postfix(&attr.handle, set.size(), set.data()) == 0);
    Require(set[0] == 0x05u);
    for (std::size_t index = 1; index < set.size(); ++index) Require(set[index] == 0u);
    Require(pthread_attr_getaffinity_np_nid_postfix(&attr.handle, 16, set.data()) == Erange);
    Require(pthread_attr_getaffinity_np_nid_postfix(&attr.handle, 32, nullptr) == Efault);
    Require(pthread_attr_getaffinity_np_nid_postfix(nullptr, 32, set.data()) == Einval);
}

void SetAffinity() {
    Attr attr;
    std::array<unsigned char, 32> set{};
    set[0] = 0x03u;
    set[1] = 0x01u;
    KernelCpumask mask = 0;
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr.handle, set.size(), set.data()) == 0);
    Require(scePthreadAttrGetaffinity(&attr.handle, &mask) == 0 && mask == 0x103u);
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr.handle, 8, set.data()) == 0);
    Require(scePthreadAttrGetaffinity(&attr.handle, &mask) == 0 && mask == 0x103u);
    set[9] = 0x01u;
    Require(pthread_attr_setaffinity_np_nid_postfix(&attr.handle, set.size(), set.data()) == Einval);
    Require(scePthreadAttrGetaffinity(&attr.handle, &mask) == 0 && mask == 0x103u);
    Require(pthread_attr_setaffinity_np_nid_postfix(nullptr, set.size(), set.data()) == Einval);
}

}

int main() {
    Solosched();
    Stackaddr();
    GetAffinity();
    SetAffinity();
    std::puts("pthread attribute solosched, stackaddr and affinity forms passed");
    return 0;
}
