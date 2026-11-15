#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "AllocationFailure.hpp"
#include <algorithm>
#include <chrono>
#include <span>
#include <cstddef>
#include <cstdio>
#include <new>
#include <string>
#include <utility>
#include <vector>

using AgcDriver::Graphics::ReferenceStorage;
using AgcDriver::Tests::SetAllocationFailureCountdown;

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

constexpr std::size_t KiB = 1024;

std::vector<std::byte> Storage(std::size_t bytes) {
    std::vector<std::byte> storage;
    storage.reserve(bytes);
    return storage;
}

bool NextAllocationFails() {
    try {
        void* probe = ::operator new(16);
        ::operator delete(probe);
        return false;
    } catch (const std::bad_alloc&) {
        return true;
    }
}

void TakeFromEmpty() {
    ReferenceStorage storage(64 * KiB);
    Expect(storage.Take(1).capacity() == 0, "an empty store hands out no storage");
    Expect(storage.IdleBytes() == 0 && storage.IdleCount() == 0, "an empty store holds nothing");
}

void ReuseSameStorage() {
    ReferenceStorage storage(64 * KiB);
    auto first = Storage(16 * KiB);
    const auto* data = first.data();
    storage.Return(std::move(first));
    Expect(storage.IdleBytes() == 16 * KiB && storage.IdleCount() == 1, "a returned buffer is held");
    Expect(storage.Take(32 * KiB).capacity() == 0, "a buffer smaller than the request is not handed out");
    Expect(storage.IdleCount() == 1, "a refused take keeps the held buffer");
    auto again = storage.Take(8 * KiB);
    Expect(again.data() == data && again.capacity() >= 16 * KiB, "a take reuses the held allocation");
    Expect(storage.IdleBytes() == 0 && storage.IdleCount() == 0, "a taken buffer leaves the store");
}

void LargerStorageTakesTheReferenceSize() {
    ReferenceStorage storage(256 * KiB);
    std::vector<std::byte> larger(64 * KiB, std::byte{0x5a});
    const auto* data = larger.data();
    storage.Return(std::move(larger));
    std::vector<std::byte> reference(16 * KiB);
    for (std::size_t index = 0; index < reference.size(); ++index) reference[index] = static_cast<std::byte>(index);
    const std::span<const std::byte> bytes(reference);
    auto taken = storage.Take(bytes.size());
    taken.assign(bytes.begin(), bytes.end());
    Expect(taken.data() == data, "a smaller reference reuses the larger held allocation");
    Expect(taken.size() == bytes.size(), "a reference copied into larger storage keeps the reference's size");
    Expect(std::equal(taken.begin(), taken.end(), bytes.begin()), "a reference copied into larger storage holds the reference's bytes");
}

void SmallestFit() {
    ReferenceStorage storage(256 * KiB);
    auto large = Storage(64 * KiB);
    auto small = Storage(16 * KiB);
    const auto* smallData = small.data();
    storage.Return(std::move(large));
    storage.Return(std::move(small));
    auto taken = storage.Take(8 * KiB);
    Expect(taken.data() == smallData, "the smallest buffer that fits is handed out");
    Expect(storage.IdleBytes() == 64 * KiB, "the larger buffer stays held");
}

void CapacityEvictsOldest() {
    ReferenceStorage storage(48 * KiB);
    auto older = Storage(32 * KiB);
    auto newer = Storage(32 * KiB);
    const auto* newerData = newer.data();
    storage.Return(std::move(older));
    storage.Return(std::move(newer));
    Expect(storage.IdleBytes() == 32 * KiB && storage.IdleCount() == 1, "the byte limit evicts the oldest buffer");
    Expect(storage.Take(1).data() == newerData, "the newest buffer survives eviction");
}

void OversizeDropped() {
    ReferenceStorage storage(16 * KiB);
    storage.Return(Storage(32 * KiB));
    Expect(storage.IdleBytes() == 0 && storage.IdleCount() == 0, "a buffer above the limit is not held");
    storage.Return(std::vector<std::byte>());
    Expect(storage.IdleCount() == 0, "a buffer without capacity is not held");
}

void SlotsEvictOldest() {
    ReferenceStorage storage(1024 * KiB);
    std::vector<std::vector<std::byte>> buffers;
    for (std::size_t index = 0; index < ReferenceStorage::Slots + 2; ++index) buffers.push_back(Storage(KiB + index));
    std::vector<std::pair<const std::byte*, std::size_t>> retained;
    for (std::size_t index = 2; index < buffers.size(); ++index) retained.emplace_back(buffers[index].data(), buffers[index].capacity());
    std::vector<std::vector<std::byte>> taken;
    taken.reserve(ReferenceStorage::Slots + 1);
    for (auto& buffer : buffers) storage.Return(std::move(buffer));
    Expect(storage.IdleCount() == ReferenceStorage::Slots, "the store holds at most its slot count");
    std::size_t expected = 0;
    for (const auto& [data, capacity] : retained) expected += capacity;
    Expect(storage.IdleBytes() == expected, "evicted buffers leave the byte count");
    for (std::size_t index = 0; index <= ReferenceStorage::Slots; ++index) taken.push_back(storage.Take(1));
    Expect(taken.back().capacity() == 0, "nothing beyond the slot count was held");
    taken.pop_back();
    std::size_t matched = 0;
    for (const auto& buffer : taken) {
        for (auto& identity : retained) {
            if (identity.first == buffer.data() && identity.second == buffer.capacity()) {
                identity.first = nullptr;
                ++matched;
                break;
            }
        }
    }
    Expect(matched == ReferenceStorage::Slots, "the held buffers are exactly the newest ones returned");
    Expect(storage.IdleBytes() == 0 && storage.IdleCount() == 0, "every held buffer was handed out");
}

void NoAllocationOnReturnOrTake() {
    static_assert(noexcept(std::declval<ReferenceStorage&>().Return(std::vector<std::byte>())));
    static_assert(noexcept(std::declval<ReferenceStorage&>().Take(0)));
    ReferenceStorage storage(48 * KiB);
    auto first = Storage(32 * KiB);
    auto second = Storage(32 * KiB);
    auto third = Storage(8 * KiB);
    SetAllocationFailureCountdown(0);
    storage.Return(std::move(first));
    storage.Return(std::move(second));
    auto taken = storage.Take(16 * KiB);
    storage.Return(std::move(third));
    const bool armed = NextAllocationFails();
    SetAllocationFailureCountdown(-1);
    Expect(armed, "return, eviction and take allocate nothing");
    Expect(taken.capacity() >= 32 * KiB, "a take under allocation failure still hands out held storage");
    Expect(storage.IdleBytes() == 8 * KiB && storage.IdleCount() == 1, "the byte count stays consistent under allocation failure");
}

}

void IdleBuffersReleased() {
    using namespace std::chrono_literals;
    const auto start = ReferenceStorage::Clock::time_point{} + 1h;
    ReferenceStorage storage(64 * KiB);
    storage.Return(Storage(16 * KiB), start);
    storage.Return(Storage(8 * KiB), start + 1s);
    Expect(storage.ReleaseIdle(start + 1999ms) == 0 && storage.IdleCount() == 2, "a buffer idle for less than the limit was released");
    Expect(storage.ReleaseIdle(start + 2s) == 16 * KiB, "a buffer idle for the limit was not released");
    Expect(storage.IdleCount() == 1 && storage.IdleBytes() == 8 * KiB, "releasing one idle buffer did not keep the newer one");
    Expect(storage.Take(4 * KiB, start + 3s).capacity() == 0, "a take handed out a buffer idle for the limit");
    Expect(storage.IdleBytes() == 0 && storage.IdleCount() == 0, "a take did not release buffers idle for the limit");
    storage.Return(Storage(16 * KiB), start + 4s);
    storage.Return(Storage(8 * KiB), start + 6s);
    Expect(storage.IdleCount() == 1 && storage.IdleBytes() == 8 * KiB, "a return did not release buffers idle for the limit");
    Expect(storage.Take(4 * KiB, start + 7s).capacity() >= 8 * KiB, "a buffer idle for less than the limit was not reused");
}

int main() {
    TakeFromEmpty();
    ReuseSameStorage();
    LargerStorageTakesTheReferenceSize();
    SmallestFit();
    CapacityEvictsOldest();
    OversizeDropped();
    SlotsEvictOldest();
    NoAllocationOnReturnOrTake();
    IdleBuffersReleased();
    if (failures != 0) return 1;
    std::printf("reference storage tests passed\n");
    return 0;
}
