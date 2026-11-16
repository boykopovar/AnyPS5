#include "prx/libSceJsc/libSceJsc.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

static void Check(bool value, const char* what, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF random check failed: %s at line %d\n", what, line);
        std::abort();
    }
}

static void TestIdentifiers() {
    const std::uint64_t first = _ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv();
    Check(first == 1, "first identifier", __LINE__);
    Check(_ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv() == 2, "second identifier", __LINE__);

    const std::uint64_t threadSafeFirst = _ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv();
    Check(threadSafeFirst == 1, "thread-safe counter is separate", __LINE__);
    Check(_ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv() == 3, "counter unaffected by thread-safe counter", __LINE__);

    constexpr int Threads = 4;
    constexpr int PerThread = 2000;
    std::vector<std::vector<std::uint64_t>> results(Threads);
    std::vector<std::thread> workers;
    for (int i = 0; i < Threads; ++i) {
        workers.emplace_back([&results, i] {
            for (int j = 0; j < PerThread; ++j)
                results[i].push_back(_ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv());
        });
    }
    for (auto& worker : workers) worker.join();
    std::vector<std::uint64_t> all;
    for (const auto& values : results) all.insert(all.end(), values.begin(), values.end());
    std::sort(all.begin(), all.end());
    for (std::size_t i = 0; i < all.size(); ++i) Check(all[i] == threadSafeFirst + 1 + i, "thread-safe identifiers are unique and dense", __LINE__);
    Check(_ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv() == threadSafeFirst + 1 + Threads * PerThread, "next thread-safe identifier", __LINE__);
}

static void TestRandomNumbers() {
    std::vector<std::uint32_t> crypto;
    std::vector<unsigned int> weak;
    std::vector<double> doubles;
    for (int i = 0; i < 256; ++i) {
        crypto.push_back(_ZN3WTF29cryptographicallyRandomNumberEv());
        weak.push_back(_ZN3WTF16weakRandomUint32Ev());
        const double value = _ZN3WTF12randomNumberEv();
        Check(value >= 0.0 && value < 1.0, "randomNumber in [0, 1)", __LINE__);
        doubles.push_back(value);
    }
    Check(std::count(crypto.begin(), crypto.end(), crypto[0]) != static_cast<long>(crypto.size()), "cryptographicallyRandomNumber varies", __LINE__);
    Check(std::count(weak.begin(), weak.end(), weak[0]) != static_cast<long>(weak.size()), "weakRandomUint32 varies", __LINE__);
    Check(std::count(doubles.begin(), doubles.end(), doubles[0]) != static_cast<long>(doubles.size()), "randomNumber varies", __LINE__);
    std::uint32_t orBits = 0;
    std::uint32_t andBits = ~0u;
    for (std::uint32_t value : crypto) {
        orBits |= value;
        andBits &= value;
    }
    Check(orBits == ~0u && andBits == 0, "cryptographicallyRandomNumber uses all 32 bits", __LINE__);
}

int main() {
    TestIdentifiers();
    TestRandomNumbers();
    return 0;
}
