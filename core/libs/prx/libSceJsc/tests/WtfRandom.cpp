#include "prx/libSceJsc/libSceJsc.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
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

struct UuidAllocationHeader {
    void* raw;
    std::size_t size;
};

static std::set<void*> uuidLiveAllocations;
static unsigned uuidAllocateCalls = 0;
static unsigned uuidDeallocateCalls = 0;
static std::size_t uuidLastAllocationSize = 0;
static bool uuidFailNextAllocation = false;

static void* UuidAllocate(std::size_t size) {
    ++uuidAllocateCalls;
    uuidLastAllocationSize = size;
    if (uuidFailNextAllocation) {
        uuidFailNextAllocation = false;
        return nullptr;
    }
    auto* header = static_cast<UuidAllocationHeader*>(std::malloc(sizeof(UuidAllocationHeader) + size));
    if (!header) std::abort();
    header->raw = header;
    header->size = size;
    void* payload = header + 1;
    uuidLiveAllocations.insert(payload);
    return payload;
}

static void UuidDeallocate(void* payload) {
    ++uuidDeallocateCalls;
    Check(uuidLiveAllocations.erase(payload) == 1, "UUID storage released once by the allocator that made it", __LINE__);
    auto* header = static_cast<UuidAllocationHeader*>(payload) - 1;
    Check(header->raw == header, "UUID allocation header intact", __LINE__);
    std::free(header->raw);
}

template<typename TException>
static bool UuidThrows(Jsc::WtfAbi::StringImpl** output) {
    try {
        _ZN3WTF25createCanonicalUUIDStringEv(output);
    } catch (const TException& error) {
        return typeid(error) == typeid(TException);
    } catch (...) {
        return false;
    }
    return false;
}

static void TestCanonicalUuids() {
    Jsc::WtfAbi::StringImpl* untouched = nullptr;
    const Jsc::WtfAbi::PersistenceAllocator allocator{&UuidAllocate, &UuidDeallocate};
    RegisterPersistenceAllocator_nid_no_patch(&allocator);
    std::set<std::string> values;
    for (unsigned sample = 0; sample < 256; ++sample) {
        Jsc::WtfAbi::StringImpl* result = nullptr;
        Check(_ZN3WTF25createCanonicalUUIDStringEv(&result) == &result, "UUID return storage", __LINE__);
        Check(result != nullptr, "UUID string allocation", __LINE__);
        Check(uuidAllocateCalls == sample + 1 && uuidLastAllocationSize == 56, "UUID storage requested from the registered allocator", __LINE__);
        Check(uuidLiveAllocations.count(result) == 1, "UUID string is the registered allocator's block", __LINE__);
        Check(result->length == 36, "UUID string length", __LINE__);
        Check(result->refCount == 2, "UUID string initial reference count", __LINE__);
        Check(result->hashAndFlags == Jsc::WtfAbi::StringImplFlag8Bit, "UUID owned Latin-1 string", __LINE__);
        Check(result->data == reinterpret_cast<const unsigned char*>(result) + 20, "UUID inline characters", __LINE__);
        std::string text(static_cast<const char*>(result->data), result->length);
        for (unsigned index = 0; index < text.size(); ++index) {
            const char character = text[index];
            if (index == 8 || index == 13 || index == 18 || index == 23) {
                Check(character == '-', "UUID separators", __LINE__);
            } else {
                Check((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'), "UUID lowercase hexadecimal", __LINE__);
            }
        }
        Check(text[14] == '4', "UUID version 4", __LINE__);
        Check(text[19] == '8' || text[19] == '9' || text[19] == 'a' || text[19] == 'b', "UUID variant", __LINE__);
        Check(values.insert(text).second, "UUID values vary", __LINE__);
        UuidDeallocate(result);
    }
    Check(uuidDeallocateCalls == 256 && uuidLiveAllocations.empty(), "UUID strings released through the registered allocator", __LINE__);

    Check(UuidThrows<std::invalid_argument>(nullptr), "null UUID return storage rejected", __LINE__);
    Check(uuidAllocateCalls == 256, "null UUID return storage rejected before allocating", __LINE__);
    Check(UuidThrows<std::invalid_argument>(reinterpret_cast<Jsc::WtfAbi::StringImpl**>(1)), "unwritable UUID return storage rejected", __LINE__);
    Check(uuidAllocateCalls == 257 && uuidDeallocateCalls == 257 && uuidLiveAllocations.empty(), "UUID storage given back when the return storage is unwritable", __LINE__);

    uuidFailNextAllocation = true;
    Check(UuidThrows<std::bad_alloc>(&untouched) && untouched == nullptr, "UUID allocation failure reported", __LINE__);
    Check(uuidAllocateCalls == 258 && uuidDeallocateCalls == 257, "failed UUID allocation is not released", __LINE__);

    const Jsc::WtfAbi::PersistenceAllocator withoutDeallocate{&UuidAllocate, nullptr};
    RegisterPersistenceAllocator_nid_no_patch(&withoutDeallocate);
    Check(UuidThrows<std::runtime_error>(&untouched) && untouched == nullptr, "UUID with an allocator that cannot release rejected", __LINE__);
    const Jsc::WtfAbi::PersistenceAllocator withoutAllocate{nullptr, &UuidDeallocate};
    RegisterPersistenceAllocator_nid_no_patch(&withoutAllocate);
    Check(UuidThrows<std::runtime_error>(&untouched) && untouched == nullptr, "UUID with an allocator that cannot allocate rejected", __LINE__);
    RegisterPersistenceAllocator_nid_no_patch(nullptr);
    Check(UuidThrows<std::runtime_error>(&untouched) && untouched == nullptr, "UUID without a registered allocator rejected", __LINE__);
    Check(uuidAllocateCalls == 258 && uuidDeallocateCalls == 257 && uuidLiveAllocations.empty(), "rejected UUID calls allocate nothing", __LINE__);
}

int main() {
    TestIdentifiers();
    TestRandomNumbers();
    TestCanonicalUuids();
    return 0;
}
