#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WebCoreAbi::LegacyDeviceMotionData;
using NKWebKit::WebCoreAbi::LegacyDeviceMotionVector;
using NKWebKit::WebCoreAbi::LegacyDeviceOrientationData;
using NKWebKit::WebCoreAbi::LegacyDeviceRef;
using NKWebKit::WebCoreAbi::LegacyOptionalBool;
using NKWebKit::WebCoreAbi::LegacyOptionalDouble;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "device motion data check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static LegacyOptionalDouble Some(double value) {
    return {true, value};
}

static LegacyOptionalDouble None(double leftover = 0) {
    return {false, leftover};
}

static bool Is(const LegacyOptionalDouble& optional, double value) {
    return optional.engaged && std::memcmp(&optional.value, &value, sizeof(value)) == 0;
}

static LegacyDeviceMotionVector* Vector(double x) {
    auto* vector = static_cast<LegacyDeviceMotionVector*>(_ZN3WTF10fastMallocEm(sizeof(LegacyDeviceMotionVector)));
    *vector = LegacyDeviceMotionVector{1, Some(x), None(), None()};
    return vector;
}

static void TestOrientation() {
    constexpr auto create = &_ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE;
    {
        const LegacyOptionalDouble alpha = Some(1.5);
        const LegacyOptionalDouble beta = Some(-0.0);
        const LegacyOptionalDouble gamma = Some(360.25);
        const LegacyOptionalBool absolute{true, true};
        const LegacyDeviceRef<LegacyDeviceOrientationData> data = create(&alpha, &beta, &gamma, &absolute);
        Require(data.ptr != nullptr && data.ptr->refCount == 1);
        Require(Is(data.ptr->alpha, 1.5) && Is(data.ptr->beta, -0.0) && Is(data.ptr->gamma, 360.25));
        Require(data.ptr->absolute.engaged && data.ptr->absolute.value);
        const LegacyDeviceRef<LegacyDeviceOrientationData> other = create(&alpha, &beta, &gamma, &absolute);
        Require(other.ptr != data.ptr);
    }
    {
        const LegacyOptionalDouble alpha = None(7);
        const LegacyOptionalDouble beta = Some(2);
        const LegacyOptionalDouble gamma = None(9);
        const LegacyOptionalBool absolute{false, true};
        const LegacyDeviceRef<LegacyDeviceOrientationData> data = create(&alpha, &beta, &gamma, &absolute);
        Require(!data.ptr->alpha.engaged && data.ptr->alpha.value == 0 && Is(data.ptr->beta, 2) && !data.ptr->gamma.engaged && data.ptr->gamma.value == 0);
        Require(!data.ptr->absolute.engaged && !data.ptr->absolute.value);
    }
    {
        const LegacyOptionalDouble alpha = Some(1);
        const LegacyOptionalDouble beta = None();
        const LegacyOptionalDouble gamma = Some(3);
        const LegacyOptionalBool absolute{true, false};
        const LegacyDeviceRef<LegacyDeviceOrientationData> data = create(&alpha, &beta, &gamma, &absolute);
        Require(Is(data.ptr->alpha, 1) && !data.ptr->beta.engaged && Is(data.ptr->gamma, 3));
        Require(data.ptr->absolute.engaged && !data.ptr->absolute.value);
        ++data.ptr->refCount;
        data.ptr->deref();
        Require(data.ptr->refCount == 1);
    }
    const LegacyOptionalDouble value = Some(1);
    const LegacyOptionalBool flag{true, true};
    Require(Throws([&] { create(nullptr, &value, &value, &flag); }));
    Require(Throws([&] { create(&value, nullptr, &value, &flag); }));
    Require(Throws([&] { create(&value, &value, nullptr, &flag); }));
    Require(Throws([&] { create(&value, &value, &value, nullptr); }));
}

static void TestMotion() {
    constexpr auto create = &_ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE;
    {
        LegacyDeviceMotionVector* acceleration = Vector(1);
        LegacyDeviceMotionVector* gravity = Vector(2);
        LegacyDeviceMotionVector* rotation = Vector(3);
        LegacyDeviceMotionVector* const first = acceleration;
        LegacyDeviceMotionVector* const second = gravity;
        LegacyDeviceMotionVector* const third = rotation;
        ++first->refCount;
        const LegacyOptionalDouble interval = Some(16.5);
        {
            const LegacyDeviceRef<LegacyDeviceMotionData> data = create(&acceleration, &gravity, &rotation, &interval);
            Require(acceleration == nullptr && gravity == nullptr && rotation == nullptr);
            Require(data.ptr->refCount == 1 && data.ptr->acceleration == first && data.ptr->accelerationIncludingGravity == second && data.ptr->rotationRate == third);
            Require(first->refCount == 2 && second->refCount == 1 && third->refCount == 1 && Is(data.ptr->interval, 16.5));
        }
        Require(first->refCount == 1 && Is(first->x, 1));
        first->deref();
    }
    {
        LegacyDeviceMotionVector* none = nullptr;
        LegacyDeviceMotionVector* rotation = Vector(3);
        const LegacyOptionalDouble interval = None(5);
        const LegacyDeviceRef<LegacyDeviceMotionData> data = create(&none, &none, &rotation, &interval);
        Require(data.ptr->acceleration == nullptr && data.ptr->accelerationIncludingGravity == nullptr && data.ptr->rotationRate != nullptr);
        Require(!data.ptr->interval.engaged && data.ptr->interval.value == 0 && rotation == nullptr);
    }
    {
        LegacyDeviceMotionVector* shared = Vector(4);
        LegacyDeviceMotionVector* const kept = shared;
        LegacyDeviceMotionVector* rotation = nullptr;
        const LegacyOptionalDouble interval = Some(1);
        const LegacyDeviceRef<LegacyDeviceMotionData> data = create(&shared, &shared, &rotation, &interval);
        Require(data.ptr->acceleration == kept && data.ptr->accelerationIncludingGravity == nullptr && shared == nullptr);
    }
    LegacyDeviceMotionVector* vector = nullptr;
    const LegacyOptionalDouble interval = Some(1);
    Require(Throws([&] { create(nullptr, &vector, &vector, &interval); }));
    Require(Throws([&] { create(&vector, nullptr, &vector, &interval); }));
    Require(Throws([&] { create(&vector, &vector, nullptr, &interval); }));
    LegacyDeviceMotionVector* kept = Vector(5);
    LegacyDeviceMotionVector* const original = kept;
    Require(Throws([&] { create(&kept, &vector, &vector, nullptr); }));
    Require(kept == original && kept->refCount == 1);
    kept->deref();
}

template<typename TFunction>
static bool ThrowsNotImplemented(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }

    template<typename T>
    T* SplitReadWriteToReadOnly() const {
        return reinterpret_cast<T*>(base + pageSize - sizeof(T) / 2);
    }

    template<typename T>
    T* SplitReadOnlyToGuard() const {
        return reinterpret_cast<T*>(base + pageSize * 2 - sizeof(T) / 2);
    }
};

static void TestGuestPointers() {
    GuardedPages pages;
    constexpr auto createOrientation = &_ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE;
    constexpr auto createMotion = &_ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE;

    const LegacyOptionalDouble validDouble = Some(1.0);
    const LegacyOptionalBool validBool{true, true};
    const auto* unmappedDouble = reinterpret_cast<const LegacyOptionalDouble*>(pages.Guard());
    const auto* splitDouble = pages.SplitReadOnlyToGuard<const LegacyOptionalDouble>();
    const auto* unmappedBool = reinterpret_cast<const LegacyOptionalBool*>(pages.Guard());
    const auto* splitBool = pages.SplitReadOnlyToGuard<const LegacyOptionalBool>();

    Require(ThrowsNotImplemented([&] { createOrientation(unmappedDouble, &validDouble, &validDouble, &validBool); }));
    Require(ThrowsNotImplemented([&] { createOrientation(splitDouble, &validDouble, &validDouble, &validBool); }));
    Require(ThrowsNotImplemented([&] { createOrientation(&validDouble, unmappedDouble, &validDouble, &validBool); }));
    Require(ThrowsNotImplemented([&] { createOrientation(&validDouble, splitDouble, &validDouble, &validBool); }));
    Require(ThrowsNotImplemented([&] { createOrientation(&validDouble, &validDouble, unmappedDouble, &validBool); }));
    Require(ThrowsNotImplemented([&] { createOrientation(&validDouble, &validDouble, splitDouble, &validBool); }));
    Require(ThrowsNotImplemented([&] { createOrientation(&validDouble, &validDouble, &validDouble, unmappedBool); }));
    Require(ThrowsNotImplemented([&] { createOrientation(&validDouble, &validDouble, &validDouble, splitBool); }));

    auto* unmappedSlot = reinterpret_cast<LegacyDeviceMotionVector**>(pages.Guard());
    auto* readOnlySlot = reinterpret_cast<LegacyDeviceMotionVector**>(pages.ReadOnly());
    auto* splitSlot = pages.SplitReadWriteToReadOnly<LegacyDeviceMotionVector*>();
    const std::size_t splitSlotHead = sizeof(LegacyDeviceMotionVector*) / 2;

    LegacyDeviceMotionVector* keptA = Vector(10);
    LegacyDeviceMotionVector* keptB = Vector(20);
    LegacyDeviceMotionVector* const origA = keptA;
    LegacyDeviceMotionVector* const origB = keptB;

    Require(ThrowsNotImplemented([&] { createMotion(unmappedSlot, &keptA, &keptB, &validDouble); }));
    Require(keptA == origA && keptB == origB);
    Require(ThrowsNotImplemented([&] { createMotion(readOnlySlot, &keptA, &keptB, &validDouble); }));
    Require(keptA == origA && keptB == origB);
    std::memset(splitSlot, 0x5A, splitSlotHead);
    Require(ThrowsNotImplemented([&] { createMotion(splitSlot, &keptA, &keptB, &validDouble); }));
    for (std::size_t i = 0; i < splitSlotHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitSlot)[i] == 0x5A);
    Require(keptA == origA && keptB == origB);

    Require(ThrowsNotImplemented([&] { createMotion(&keptA, unmappedSlot, &keptB, &validDouble); }));
    Require(keptA == origA && keptB == origB);
    Require(ThrowsNotImplemented([&] { createMotion(&keptA, readOnlySlot, &keptB, &validDouble); }));
    Require(keptA == origA && keptB == origB);
    std::memset(splitSlot, 0x6B, splitSlotHead);
    Require(ThrowsNotImplemented([&] { createMotion(&keptA, splitSlot, &keptB, &validDouble); }));
    for (std::size_t i = 0; i < splitSlotHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitSlot)[i] == 0x6B);
    Require(keptA == origA && keptB == origB);

    Require(ThrowsNotImplemented([&] { createMotion(&keptA, &keptB, unmappedSlot, &validDouble); }));
    Require(keptA == origA && keptB == origB);
    Require(ThrowsNotImplemented([&] { createMotion(&keptA, &keptB, readOnlySlot, &validDouble); }));
    Require(keptA == origA && keptB == origB);
    std::memset(splitSlot, 0x7C, splitSlotHead);
    Require(ThrowsNotImplemented([&] { createMotion(&keptA, &keptB, splitSlot, &validDouble); }));
    for (std::size_t i = 0; i < splitSlotHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitSlot)[i] == 0x7C);
    Require(keptA == origA && keptB == origB);

    Require(ThrowsNotImplemented([&] { createMotion(&keptA, &keptB, &keptB, unmappedDouble); }));
    Require(keptA == origA && keptB == origB);
    Require(ThrowsNotImplemented([&] { createMotion(&keptA, &keptB, &keptB, splitDouble); }));
    Require(keptA == origA && keptB == origB);

    keptA->deref();
    keptB->deref();

    LegacyDeviceMotionVector* triple = Vector(30);
    LegacyDeviceMotionVector* const origTriple = triple;
    {
        const LegacyDeviceRef<LegacyDeviceMotionData> data = createMotion(&triple, &triple, &triple, &validDouble);
        Require(triple == nullptr);
        Require(data.ptr->acceleration == origTriple && data.ptr->accelerationIncludingGravity == nullptr && data.ptr->rotationRate == nullptr);
    }
}

int main() {
    TestOrientation();
    TestMotion();
    TestGuestPointers();
    std::puts("libSceNKWebKit device motion data tests passed");
    return 0;
}
