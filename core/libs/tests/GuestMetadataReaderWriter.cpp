#include "prx/libSceMetadataReaderWriter/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "ProcessParametersFixture.hpp"

using SceMetadataReaderWriter::Metadata;
using SceMetadataReaderWriter::Value;

static void Require(bool value) { if (!value) std::abort(); }

static void* APS5_VABI TestAllocate(std::size_t) { return nullptr; }
static void APS5_VABI TestRelease(void*) {}

alignas(64) static std::array<std::byte, 64> g_heapStorage{};
static std::size_t g_heapAllocations = 0;
static void* g_heapReleased = nullptr;

static void* APS5_VABI HeapAllocate(std::size_t bytes) {
    Require(bytes <= g_heapStorage.size());
    ++g_heapAllocations;
    return g_heapStorage.data();
}

static void APS5_VABI HeapRelease(void* pointer) { g_heapReleased = pointer; }
static void* APS5_VABI HeapAllocateZeroed(std::size_t count, std::size_t bytes) { return HeapAllocate(count * bytes); }
static void* APS5_VABI HeapReallocate(void*, std::size_t bytes) { return HeapAllocate(bytes); }
static void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return HeapAllocate(bytes); }
static void* APS5_VABI HeapRealign(void*, std::size_t bytes, std::size_t) { return HeapAllocate(bytes); }
static int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = HeapAllocate(bytes);
    return 0;
}

template <typename TValue, std::size_t TSize>
static void Write(std::array<std::byte, TSize>& data, std::size_t offset, TValue value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

static void InstallHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    Write(process, 0, std::uint64_t{0x40});
    Write(process, 8, std::uint32_t{0x4942524f});
    Write(process, 0x38, libc.data());
    Write(libc, 0, std::uint64_t{0x38});
    Write(libc, 0x30, replacement.data());
    Write(replacement, 0, std::uint64_t{0x78});
    Write(replacement, 8, std::uint64_t{2});
    Write(replacement, 0x20, &HeapAllocate);
    Write(replacement, 0x28, &HeapRelease);
    Write(replacement, 0x30, &HeapAllocateZeroed);
    Write(replacement, 0x38, &HeapReallocate);
    Write(replacement, 0x40, &HeapAlign);
    Write(replacement, 0x48, &HeapRealign);
    Write(replacement, 0x50, &HeapPosixAlign);
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
}

template <typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

static Metadata Filled() {
    Metadata metadata;
    std::memset(&metadata, 0xA5, sizeof(metadata));
    return metadata;
}

using Destructor = void (APS5_VABI*)(Metadata*);

static Destructor Slot(const Metadata& metadata, std::size_t index) {
    return reinterpret_cast<Destructor>(const_cast<void*>(metadata.vtable[index]));
}

static void RequireInitial(const Metadata& metadata) {
    Require(metadata.vtable != nullptr);
    Require(metadata.vtable[-2] == nullptr);
    Require(metadata.vtable[-1] == nullptr);
    Require(metadata.vtable[0] != nullptr && metadata.vtable[1] != nullptr && metadata.vtable[0] != metadata.vtable[1]);
    Require(metadata.marker == -1);
    Require(metadata.reserved == 0);
    Require(metadata.fields == nullptr);
}

static void TestConstruction() {
    Metadata first = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC1Ev(&first);
    RequireInitial(first);
    Require(first.allocate != nullptr && first.release != nullptr);
    Metadata second = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC2Ev(&second);
    Require(std::memcmp(&first, &second, sizeof(first)) == 0);
    Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(&first) == 0);
    _ZNK23sceMetadataReaderWriter8Metadata9dumpValueEv(&first);

    Metadata custom = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(&custom, TestAllocate, TestRelease);
    RequireInitial(custom);
    Require(custom.allocate == TestAllocate && custom.release == TestRelease);
    Metadata empty = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC2EPFPvmEPFvS1_E(&empty, nullptr, nullptr);
    RequireInitial(empty);
    Require(empty.allocate == nullptr && empty.release == nullptr);
    _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(&empty, TestAllocate, nullptr);
    Require(empty.allocate == TestAllocate && empty.release == nullptr);
    Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(&empty) == 0);

    custom.marker = 7;
    custom.reserved = 5;
    Metadata copy = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_(&copy, &custom);
    Require(std::memcmp(&copy, &first, sizeof(copy)) == 0);
    copy = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_(&copy, &custom);
    Require(std::memcmp(&copy, &first, sizeof(copy)) == 0);
    copy = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_PFPvmEPFvS3_E(&copy, &first, nullptr, nullptr);
    RequireInitial(copy);
    Require(copy.allocate == nullptr && copy.release == nullptr);
    copy = Filled();
    _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_PFPvmEPFvS3_E(&copy, &first, TestAllocate, TestRelease);
    RequireInitial(copy);
    Require(copy.allocate == TestAllocate && copy.release == TestRelease);

    Metadata target = custom;
    Require(_ZN23sceMetadataReaderWriter8MetadataaSERKS0_(&target, &first) == &target);
    Require(std::memcmp(&target, &custom, sizeof(target)) == 0);
    target = first;
    Require(_ZN23sceMetadataReaderWriter8MetadataaSERKS0_(&target, &custom) == &target);
    Require(std::memcmp(&target, &first, sizeof(target)) == 0);
    Metadata before = target;
    Require(_ZN23sceMetadataReaderWriter8MetadataaSERKS0_(&target, &target) == &target);
    Require(std::memcmp(&target, &before, sizeof(target)) == 0);

    Metadata destroyed = custom;
    _ZN23sceMetadataReaderWriter8MetadataD1Ev(&destroyed);
    Require(std::memcmp(&destroyed, &custom, sizeof(destroyed)) == 0);
    _ZN23sceMetadataReaderWriter8MetadataD2Ev(&destroyed);
    Require(std::memcmp(&destroyed, &custom, sizeof(destroyed)) == 0);

    auto* heap = static_cast<Metadata*>(_Znwm_nid_postfix(sizeof(Metadata)));
    Require(heap == reinterpret_cast<Metadata*>(g_heapStorage.data()) && g_heapAllocations == 1);
    _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(heap, TestAllocate, TestRelease);
    _ZN23sceMetadataReaderWriter8MetadataD0Ev(heap);
    Require(g_heapReleased == heap && g_heapAllocations == 1);

    Slot(destroyed, 0)(&destroyed);
    Require(std::memcmp(&destroyed, &custom, sizeof(destroyed)) == 0);
    g_heapReleased = nullptr;
    heap = static_cast<Metadata*>(_Znwm_nid_postfix(sizeof(Metadata)));
    Require(heap == reinterpret_cast<Metadata*>(g_heapStorage.data()) && g_heapAllocations == 2);
    _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(heap, TestAllocate, TestRelease);
    Slot(*heap, 1)(heap);
    Require(g_heapReleased == heap && g_heapAllocations == 2);
}

static void TestUnsupportedAndNull() {
    Metadata metadata;
    _ZN23sceMetadataReaderWriter8MetadataC1Ev(&metadata);
    Metadata stored = metadata;
    int storage = 0;
    stored.fields = &storage;
    Metadata target;
    RequireThrows<std::runtime_error>([&] { _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(&stored); });
    RequireThrows<std::runtime_error>([&] { _ZNK23sceMetadataReaderWriter8Metadata9dumpValueEv(&stored); });
    RequireThrows<std::runtime_error>([&] { _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_(&target, &stored); });
    RequireThrows<std::runtime_error>([&] { _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_PFPvmEPFvS3_E(&target, &stored, nullptr, nullptr); });
    RequireThrows<std::runtime_error>([&] { _ZN23sceMetadataReaderWriter8MetadataaSERKS0_(&metadata, &stored); });
    RequireThrows<std::runtime_error>([&] { _ZN23sceMetadataReaderWriter8MetadataaSERKS0_(&stored, &metadata); });
    RequireThrows<std::runtime_error>([&] { _ZN23sceMetadataReaderWriter8MetadataD1Ev(&stored); });
    RequireThrows<std::runtime_error>([&] { Slot(stored, 0)(&stored); });
    RequireThrows<std::runtime_error>([&] { Slot(stored, 1)(&stored); });

    RequireThrows<std::invalid_argument>([] { _ZN23sceMetadataReaderWriter8MetadataC1Ev(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN23sceMetadataReaderWriter8MetadataC2EPFPvmEPFvS1_E(nullptr, nullptr, nullptr); });
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_(&target, nullptr); });
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_(nullptr, &metadata); });
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8MetadataaSERKS0_(&metadata, nullptr); });
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8MetadataaSERKS0_(nullptr, &metadata); });
    RequireThrows<std::invalid_argument>([] { _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNK23sceMetadataReaderWriter8Metadata9dumpValueEv(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN23sceMetadataReaderWriter8MetadataD1Ev(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN23sceMetadataReaderWriter8MetadataD0Ev(nullptr); });
}

struct PaddedValue {
    Value value;
    std::uint64_t tail;
};

static PaddedValue Pattern(std::int32_t type) {
    PaddedValue destination;
    std::memset(&destination, 0x55, sizeof(destination));
    destination.value.type = type;
    return destination;
}

static void RequireAssigned(std::int32_t type, bool keepsSecond) {
    Value source{type, 0xA1A2A3A4u, 0x1111111111111111ull, 0x2222222222222222ull};
    PaddedValue target = Pattern(0x55555555);
    Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&target.value, &source) == &target.value);
    Require(target.value.type == type && target.value.padding == 0x55555555u && target.value.first == 0x1111111111111111ull);
    Require(target.value.second == (keepsSecond ? 0x5555555555555555ull : 0x2222222222222222ull));
    Require(target.tail == 0x5555555555555555ull);
    Require(source.type == type && source.padding == 0xA1A2A3A4u && source.first == 0x1111111111111111ull && source.second == 0x2222222222222222ull);
}

static void RequireSelfAssigned(std::int32_t type) {
    PaddedValue self = Pattern(type);
    PaddedValue before = self;
    Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&self.value, &self.value) == &self.value);
    Require(std::memcmp(&self, &before, sizeof(self)) == 0);
}

static void TestValueAssignment() {
    for (std::int32_t type = 0; type <= 3; ++type) RequireAssigned(type, false);
    for (std::int32_t type : {4, 17, 255, 0x7FFFFFFF, -1, static_cast<std::int32_t>(0x80000000)}) RequireAssigned(type, true);
    RequireSelfAssigned(2);
    RequireSelfAssigned(17);
    Value self{2, 0, 5, 6};
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter5ValueaSERS0_(nullptr, &self); });
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter5ValueaSERS0_(&self, nullptr); });
}

int main() {
    InstallHeap();
    TestConstruction();
    TestUnsupportedAndNull();
    TestValueAssignment();
    return 0;
}
