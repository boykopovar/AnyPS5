#include "prx/libSceLibcInternal/include/FacetReferences.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal locale::facet references: %s\n", message);
    std::exit(1);
}

template<typename TCall>
bool ThrowsInvalidArgument(TCall&& call) {
    try {
        call();
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

} // namespace

int main() {
    GuestLocale::Facet facet{nullptr, 0, 0x5a5a5a5au};
    _ZNSt6locale5facet7_IncrefEv_nid_postfix(&facet);
    _ZNSt6locale5facet7_IncrefEv_nid_postfix(&facet);
    Require(facet.references == 2 && facet.vtable == nullptr && facet.reserved == 0x5a5a5a5au, "_Incref adds one to the 32-bit count and touches nothing else");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == nullptr && facet.references == 1, "_Decref returns null while references remain");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == &facet && facet.references == 0, "_Decref returns the facet when the last reference goes");

    constexpr int Threads = 8;
    constexpr int Rounds = 20000;
    std::vector<std::thread> workers;
    for (int index = 0; index < Threads; ++index)
        workers.emplace_back([&] { for (int round = 0; round < Rounds; ++round) _ZNSt6locale5facet7_IncrefEv_nid_postfix(&facet); });
    for (auto& worker : workers) worker.join();
    Require(facet.references == Threads * Rounds, "concurrent _Incref calls are not lost");
    std::atomic<int> released{0};
    workers.clear();
    for (int index = 0; index < Threads; ++index)
        workers.emplace_back([&] {
            for (int round = 0; round < Rounds; ++round)
                if (_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == &facet) released.fetch_add(1);
        });
    for (auto& worker : workers) worker.join();
    Require(facet.references == 0 && released.load() == 1, "exactly one concurrent _Decref sees the last reference go");

    Require(ThrowsInvalidArgument([] { _ZNSt6locale5facet7_IncrefEv_nid_postfix(nullptr); }) &&
        ThrowsInvalidArgument([] { _ZNSt6locale5facet7_DecrefEv_nid_postfix(nullptr); }), "a null facet throws std::invalid_argument");
    return 0;
}
