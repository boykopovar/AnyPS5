#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* kind, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal type_info destructors: %s: %s\n", kind, message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

using Destructor = void (APS5_VABI*)(void*);

constexpr std::size_t MaxRecordSize = 56;

struct Kind {
    const char* name;
    std::size_t size;
    Destructor base;
    Destructor complete;
    Destructor deleting;
};

void Fill(unsigned char* record, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) record[i] = static_cast<unsigned char>(0xA5 ^ i);
}

void CheckInPlace(const Kind& kind, Destructor destructor, const char* label) {
    std::array<unsigned char, MaxRecordSize> expected{};
    Fill(expected.data(), kind.size);
    std::array<unsigned char, MaxRecordSize> record = expected;
    releases = 0;
    destructor(record.data());
    Require(std::memcmp(record.data(), expected.data(), MaxRecordSize) == 0, kind.name, label);
    Require(releases == 0, kind.name, "an in-place destructor does not release storage");
    destructor(record.data());
    Require(std::memcmp(record.data(), expected.data(), MaxRecordSize) == 0 && releases == 0, kind.name,
        "a second in-place destruction changes nothing");
}

void CheckDeleting(const Kind& kind) {
    auto* record = static_cast<unsigned char*>(_Znwm_nid_postfix(kind.size));
    Require(record != nullptr, kind.name, "operator new returns storage");
    Fill(record, kind.size);
    releases = 0;
    lastReleased = nullptr;
    kind.deleting(record);
    Require(releases == 1 && lastReleased == record, kind.name, "D0 releases the object once through operator delete");
    releases = 0;
    kind.deleting(nullptr);
    Require(releases == 0, kind.name, "D0 of a null object releases nothing, as operator delete of null does");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    const std::array<Kind, 10> kinds{{
        {"__fundamental_type_info", 16, &sceLibcInternalFundamentalTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalFundamentalTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalFundamentalTypeInfoDeletingDestructor_nid_no_patch},
        {"__array_type_info", 16, &sceLibcInternalArrayTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalArrayTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalArrayTypeInfoDeletingDestructor_nid_no_patch},
        {"__function_type_info", 16, &sceLibcInternalFunctionTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalFunctionTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalFunctionTypeInfoDeletingDestructor_nid_no_patch},
        {"__enum_type_info", 16, &sceLibcInternalEnumTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalEnumTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalEnumTypeInfoDeletingDestructor_nid_no_patch},
        {"__class_type_info", 16, &sceLibcInternalClassTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalClassTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalClassTypeInfoDeletingDestructor_nid_no_patch},
        {"__si_class_type_info", 24, &sceLibcInternalSiClassTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalSiClassTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalSiClassTypeInfoDeletingDestructor_nid_no_patch},
        {"__vmi_class_type_info", 56, &sceLibcInternalVmiClassTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalVmiClassTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalVmiClassTypeInfoDeletingDestructor_nid_no_patch},
        {"__pbase_type_info", 32, &sceLibcInternalPbaseTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalPbaseTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalPbaseTypeInfoDeletingDestructor_nid_no_patch},
        {"__pointer_type_info", 32, &sceLibcInternalPointerTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalPointerTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalPointerTypeInfoDeletingDestructor_nid_no_patch},
        {"__pointer_to_member_type_info", 40, &sceLibcInternalPointerToMemberTypeInfoBaseDestructor_nid_no_patch,
         &sceLibcInternalPointerToMemberTypeInfoCompleteDestructor_nid_no_patch,
         &sceLibcInternalPointerToMemberTypeInfoDeletingDestructor_nid_no_patch}
    }};
    for (const auto& kind : kinds) {
        Require(kind.size <= MaxRecordSize, kind.name, "record fits the test buffer");
        CheckInPlace(kind, kind.base, "D2 leaves the object unchanged");
        CheckInPlace(kind, kind.complete, "D1 leaves the object unchanged");
        CheckDeleting(kind);
    }

    std::puts("libc internal type_info destructor checks passed");
    return 0;
}
