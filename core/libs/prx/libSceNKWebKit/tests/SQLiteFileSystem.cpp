#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SQLiteFileSystemExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

using NKWebKit::WtfAbi::String;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "SQLiteFileSystem check failed at line %u\n", location.line());
        std::abort();
    }
}

std::unordered_set<void*> guestAllocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Check(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (pointer) Check(guestAllocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    if (result) guestAllocations.insert(result);
    return result;
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) {
    return Allocate_nid_no_patch(size);
}

void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) {
    return Reallocate_nid_no_patch(pointer, size);
}

int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String Make16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

std::string Latin1(const String& string) {
    Check(string.impl != nullptr);
    Check((string.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(string.impl->data), string.impl->length);
}

std::string Hash(const String& fileName) {
    return Latin1(_ZN7WebCore16SQLiteFileSystem22computeHashForFileNameERKN3WTF6StringE(&fileName));
}

void TestAsciiName() {
    Check(Hash(Make("abc")) == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    Check(Hash(Make("IndexedDB")) == "0165D027F544FD19809785832B483CEFF772FBCA465A8087FDD2BA34C404F3AD");
}

void TestEmptyAndNullNames() {
    const char* emptyHash = "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
    Check(Hash(Make("")) == emptyHash);
    Check(Hash(String()) == emptyHash);
}

void TestNameIsHashedAsUtf8() {
    Check(Hash(Make16(u"café \U0001F600")) == "043764DF773AC7CEEA6175E1498893E6EE33E79885288417CC1D75CBA6094827");
}

void TestNullArgumentThrows() {
    bool threw = false;
    try {
        _ZN7WebCore16SQLiteFileSystem22computeHashForFileNameERKN3WTF6StringE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw);
}

}  // namespace

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestAsciiName();
    TestEmptyAndNullNames();
    TestNameIsHashedAsUtf8();
    TestNullArgumentThrows();
    Check(guestAllocations.empty());
    std::puts("SQLiteFileSystem tests passed");
    return 0;
}
