#include "prx/libSceMetadataReaderWriter/include/Api.hpp"
#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
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

static const char* const g_serializedConstructed =
    "{\"field_list\":[],\"mimeType\":4294967295,\"mimeTypeFormatVersion\":0,\"parserId\":4294967295,\"promoterFormatVersion\":0}";

static std::string JsonText(SceJson2::Value* value) {
    SceJson2::String text{};
    _ZN3sce4Json6StringC1Ev(&text);
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(value, &text) == 0);
    std::string result(_ZNK3sce4Json6String5c_strEv(&text));
    _ZN3sce4Json6StringD1Ev(&text);
    return result;
}

static Metadata Constructed() {
    Metadata metadata;
    _ZN23sceMetadataReaderWriter8MetadataC1Ev(&metadata);
    return metadata;
}

static void RequireSerializesConstructed(Metadata& metadata, SceJson2::Value& destination) {
    const Metadata before = metadata;
    Require(_ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE(&metadata, &destination) == 0);
    Require(std::memcmp(&metadata, &before, sizeof(metadata)) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&destination) == SceJson2::TypeObject);
    Require(_ZNK3sce4Json5Value5countEv(&destination) == 5);
    Require(JsonText(&destination) == g_serializedConstructed);
}

static void RequireSerializeRejects(Metadata& metadata, SceJson2::Value& destination) {
    const Metadata before = metadata;
    const std::string text = JsonText(&destination);
    RequireThrows<std::runtime_error>([&] { _ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE(&metadata, &destination); });
    Require(std::memcmp(&metadata, &before, sizeof(metadata)) == 0);
    Require(JsonText(&destination) == text);
}

static void TestSerialize() {
    Metadata metadata = Constructed();
    Metadata custom;
    _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(&custom, TestAllocate, TestRelease);
    SceJson2::Value null{};
    _ZN3sce4Json5ValueC1Ev(&null);
    SceJson2::Value object{};
    _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&object, SceJson2::TypeObject);
    SceJson2::Value integer{};
    _ZN3sce4Json5ValueC1El(&integer, 7);
    SceJson2::Value text{};
    _ZN3sce4Json5ValueC1EPKc(&text, "ab");
    SceJson2::Value fresh{};
    _ZN3sce4Json5ValueC1Ev(&fresh);
    for (SceJson2::Value* destination : {&null, &object, &integer, &text}) RequireSerializesConstructed(metadata, *destination);
    RequireSerializesConstructed(metadata, null);
    RequireSerializesConstructed(custom, fresh);

    SceJson2::Value boolean{};
    _ZN3sce4Json5ValueC1Eb(&boolean, true);
    SceJson2::Value unsignedInteger{};
    _ZN3sce4Json5ValueC1Em(&unsignedInteger, 7);
    SceJson2::Value real{};
    _ZN3sce4Json5ValueC1Ed(&real, 1.5);
    SceJson2::Value array{};
    _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&array, SceJson2::TypeArray);
    for (SceJson2::Value* destination : {&boolean, &unsignedInteger, &real, &array}) RequireSerializeRejects(metadata, *destination);

    Metadata deserialized = Constructed();
    deserialized.marker = 0;
    RequireSerializeRejects(deserialized, fresh);
    Metadata reserved = Constructed();
    reserved.reserved = 1;
    RequireSerializeRejects(reserved, fresh);
    Metadata stored = Constructed();
    int storage = 0;
    stored.fields = &storage;
    RequireSerializeRejects(stored, fresh);

    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE(nullptr, &fresh); });
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE(&metadata, nullptr); });

    for (SceJson2::Value* value : {&null, &object, &integer, &text, &fresh, &boolean, &unsignedInteger, &real, &array}) _ZN3sce4Json5ValueD1Ev(value);
}

static void RequireDeserializes(Metadata metadata, SceJson2::Value& source, std::int64_t marker) {
    Metadata expected = metadata;
    expected.marker = marker;
    const std::string text = JsonText(&source);
    Require(_ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_(&source, &metadata) == 0);
    Require(std::memcmp(&metadata, &expected, sizeof(metadata)) == 0);
    Require(JsonText(&source) == text);
    Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(&metadata) == 0);
}

static void RequireDeserializeRejects(Metadata metadata, SceJson2::Value& source) {
    const Metadata before = metadata;
    const std::string text = JsonText(&source);
    RequireThrows<std::runtime_error>([&] { _ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_(&source, &metadata); });
    Require(std::memcmp(&metadata, &before, sizeof(metadata)) == 0);
    Require(JsonText(&source) == text);
}

static SceJson2::Value* Member(SceJson2::Value& object, const char* key) {
    SceJson2::String name{};
    _ZN3sce4Json6StringC1EPKc(&name, key);
    SceJson2::Value* member = _ZN3sce4Json6ObjectixERKNS0_6StringE(_ZN3sce4Json5Value11referObjectEv(&object), &name);
    _ZN3sce4Json6StringD1Ev(&name);
    return member;
}

static void TestDeserialize() {
    Metadata metadata = Constructed();
    Metadata custom;
    _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(&custom, TestAllocate, TestRelease);
    SceJson2::Value serialized{};
    _ZN3sce4Json5ValueC1Ev(&serialized);
    Require(_ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE(&metadata, &serialized) == 0);
    RequireDeserializes(metadata, serialized, -1);
    RequireDeserializes(custom, serialized, -1);

    SceJson2::Value object{};
    _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&object, SceJson2::TypeObject);
    SceJson2::Value null{};
    _ZN3sce4Json5ValueC1Ev(&null);
    SceJson2::Value seven{};
    _ZN3sce4Json5ValueC1El(&seven, 7);
    SceJson2::Value zero{};
    _ZN3sce4Json5ValueC1El(&zero, 0);
    SceJson2::Value text{};
    _ZN3sce4Json5ValueC1EPKc(&text, "ab");
    SceJson2::Value empty{};
    _ZN3sce4Json5ValueC1EPKc(&empty, "");
    for (SceJson2::Value* source : {&object, &null, &seven, &zero, &text, &empty}) {
        RequireDeserializes(metadata, *source, 0);
        RequireDeserializes(custom, *source, 0);
    }

    Metadata deserialized = metadata;
    Require(_ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_(&null, &deserialized) == 0);
    RequireDeserializeRejects(deserialized, null);
    RequireDeserializeRejects(deserialized, serialized);
    Metadata reserved = metadata;
    reserved.reserved = 1;
    RequireDeserializeRejects(reserved, null);
    Metadata stored = metadata;
    int storage = 0;
    stored.fields = &storage;
    RequireDeserializeRejects(stored, null);

    SceJson2::Value boolean{};
    _ZN3sce4Json5ValueC1Eb(&boolean, false);
    SceJson2::Value unsignedInteger{};
    _ZN3sce4Json5ValueC1Em(&unsignedInteger, 0);
    SceJson2::Value real{};
    _ZN3sce4Json5ValueC1Ed(&real, 0);
    SceJson2::Value array{};
    _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&array, SceJson2::TypeArray);
    SceJson2::Value other{};
    _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&other, SceJson2::TypeObject);
    _ZN3sce4Json5Value3setEl(Member(other, "a"), 1);
    SceJson2::Value changedIdentifier{};
    _ZN3sce4Json5ValueC1ERKS1_(&changedIdentifier, &serialized);
    _ZN3sce4Json5Value3setEl(Member(changedIdentifier, "parserId"), 5);
    SceJson2::Value unsignedIdentifier{};
    _ZN3sce4Json5ValueC1ERKS1_(&unsignedIdentifier, &serialized);
    _ZN3sce4Json5Value3setEm(Member(unsignedIdentifier, "mimeType"), 0xFFFFFFFFu);
    SceJson2::Value extraMember{};
    _ZN3sce4Json5ValueC1ERKS1_(&extraMember, &serialized);
    _ZN3sce4Json5Value3setEl(Member(extraMember, "title"), 0);
    SceJson2::Value missingMember{};
    _ZN3sce4Json5ValueC1ERKS1_(&missingMember, &serialized);
    SceJson2::String promoter{};
    _ZN3sce4Json6StringC1EPKc(&promoter, "promoterFormatVersion");
    _ZN3sce4Json6Object5eraseERKNS0_6StringE(_ZN3sce4Json5Value11referObjectEv(&missingMember), &promoter);
    _ZN3sce4Json6StringD1Ev(&promoter);
    SceJson2::Value filledList{};
    _ZN3sce4Json5ValueC1ERKS1_(&filledList, &serialized);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(_ZN3sce4Json5Value10referArrayEv(Member(filledList, "field_list")), &zero);
    for (SceJson2::Value* source : {&boolean, &unsignedInteger, &real, &array, &other, &changedIdentifier, &unsignedIdentifier, &extraMember, &missingMember, &filledList})
        RequireDeserializeRejects(metadata, *source);

    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_(nullptr, &metadata); });
    RequireThrows<std::invalid_argument>([&] { _ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_(&null, nullptr); });

    for (SceJson2::Value* value : {&serialized, &object, &null, &seven, &zero, &text, &empty, &boolean, &unsignedInteger, &real, &array, &other,
             &changedIdentifier, &unsignedIdentifier, &extraMember, &missingMember, &filledList})
        _ZN3sce4Json5ValueD1Ev(value);
}

int main() {
    InstallHeap();
    TestConstruction();
    TestUnsupportedAndNull();
    TestValueAssignment();
    TestSerialize();
    TestDeserialize();
    return 0;
}
