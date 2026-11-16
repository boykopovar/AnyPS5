#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <bit>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using CategoryName = const char* (APS5_VABI*)(const void*) noexcept;
using VtableName = const char* (APS5_VABI*)(const LibcException::CategoryObject*);
using VtableMessage = GuestLocale::String* (APS5_VABI*)(GuestLocale::String*, const LibcException::CategoryObject*, int);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal constant members: %s\n", message);
    std::abort();
}

const char* LibcName(const LibcException::CategoryObject* category) {
    return std::bit_cast<VtableName>(static_cast<const void* const*>(category->vtable)[2])(category);
}

void CheckName(CategoryName name, const LibcException::CategoryObject* category, const char* expected, const char* message) {
    const int other = 0;
    alignas(8) unsigned char unaligned[16]{};
    const char* first = name(category);
    Require(first != nullptr && std::strcmp(first, expected) == 0, message);
    Require(name(nullptr) == first && name(&other) == first && name(unaligned + 1) == first, "the name does not depend on the object");
    if (category) Require(std::strcmp(LibcName(category), first) == 0, "the name matches the libc category vtable");
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

void RegisterHeap() {
    static std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
}

void CheckFutureErrorWhat() {
    static constexpr const char* expected[] {"broken promise", "future already retrieved", "promise already satisfied", "no state"};
    const auto* category = _ZSt15future_categoryv_nid_postfix();
    const auto message = std::bit_cast<VtableMessage>(static_cast<const void* const*>(category->vtable)[3]);
    for (int code = 1; code <= 4; ++code) {
        const char* what = _ZSt22_Get_future_error_whati_nid_postfix(code);
        Require(what != nullptr && std::strcmp(what, expected[code - 1]) == 0, "_Get_future_error_what names the future error");
        Require(_ZSt22_Get_future_error_whati_nid_postfix(code) == what, "_Get_future_error_what returns the same string each time");
        Require(sceLibcInternalFutureErrorMap_nid_no_patch(code) == what, "_Future_error_map returns the string _Get_future_error_what does");
        GuestLocale::String text{};
        message(&text, category, code);
        const bool heap = text.capacity > GuestLocale::StringInlineCapacity;
        const char* data = heap ? text.pointer : text.buffer;
        Require(text.size == std::strlen(what) && std::memcmp(data, what, text.size) == 0, "_Get_future_error_what matches the libc future category message");
        if (heap) _ZdlPv_nid_postfix(text.pointer);
    }
    for (int code : {0, 5, -1, 6, 22, 256, 257, 0x10001, INT_MIN, INT_MAX}) {
        Require(_ZSt22_Get_future_error_whati_nid_postfix(code) == nullptr, "_Get_future_error_what is null outside 1 to 4");
        Require(sceLibcInternalFutureErrorMap_nid_no_patch(code) == nullptr, "_Future_error_map is null outside 1 to 4");
    }
}

void CheckCodecvtBase() {
    const int other = 0;
    alignas(8) unsigned char unaligned[16]{};
    Require(sceLibcInternalCodecvtBaseMaxLength_nid_no_patch(nullptr) == 1, "codecvt_base::do_max_length returns 1");
    Require(sceLibcInternalCodecvtBaseMaxLength_nid_no_patch(&other) == 1, "codecvt_base::do_max_length ignores the facet");
    Require(sceLibcInternalCodecvtBaseMaxLength_nid_no_patch(unaligned + 1) == 1, "codecvt_base::do_max_length ignores unaligned facets");
    Require(sceLibcInternalCodecvtBaseEncoding_nid_no_patch(nullptr) == 1, "codecvt_base::do_encoding returns 1");
    Require(sceLibcInternalCodecvtBaseEncoding_nid_no_patch(&other) == 1, "codecvt_base::do_encoding ignores the facet");
    Require(sceLibcInternalCodecvtBaseEncoding_nid_no_patch(unaligned + 1) == 1, "codecvt_base::do_encoding ignores unaligned facets");
}

}

int main() {
    const auto* systemCategory = _ZSt15system_categoryv_nid_postfix();
    const auto* genericCategory = _ZSt16generic_categoryv_nid_postfix();
    const auto* futureCategory = _ZSt15future_categoryv_nid_postfix();
    Require(_ZSt15system_categoryv_nid_postfix() == systemCategory, "system_category returns the same singleton");
    Require(_ZSt16generic_categoryv_nid_postfix() == genericCategory, "generic_category returns the same singleton");
    Require(_ZSt15future_categoryv_nid_postfix() == futureCategory, "future_category returns the same singleton");
    CheckName(sceLibcInternalSystemErrorCategoryName_nid_no_patch, systemCategory, "system", "_System_error_category::name is system");
    CheckName(sceLibcInternalGenericErrorCategoryName_nid_no_patch, genericCategory, "generic", "_Generic_error_category::name is generic");
    CheckName(sceLibcInternalFutureErrorCategoryName_nid_no_patch, futureCategory, "future", "_Future_error_category::name is future");
    CheckName(sceLibcInternalIostreamErrorCategoryName_nid_no_patch, nullptr, "iostream", "_Iostream_error_category::name is iostream");
    CheckCodecvtBase();
    RegisterHeap();
    CheckFutureErrorWhat();
    std::puts("libc internal constant members passed");
    return 0;
}
