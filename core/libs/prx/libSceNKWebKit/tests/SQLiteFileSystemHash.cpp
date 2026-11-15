#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <unordered_set>

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static_assert(sizeof(String) == 8);
static_assert(offsetof(String, impl) == 0);
static_assert(!std::is_trivially_copyable_v<String>);
static_assert(sizeof(StringImpl) == 24);
static_assert(offsetof(StringImpl, refCount) == 0);
static_assert(offsetof(StringImpl, length) == 4);
static_assert(offsetof(StringImpl, data) == 8);
static_assert(offsetof(StringImpl, hashAndFlags) == 16);

static void Check(bool value, const char* name, int line) {
    if (!value) {
        std::fprintf(stderr, "SQLiteFileSystem::computeHashForFileName check failed for %s at line %d\n", name, line);
        std::abort();
    }
}
#define Require(value, name) Check((value), (name), __LINE__)

static std::unordered_set<void*> guestAllocations;
static std::size_t guestAllocationCount = 0;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) {
        guestAllocations.insert(pointer);
        ++guestAllocationCount;
    }
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1, "guest free");
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

static const char kEmpty[] = "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
static const char kAbc[] = "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";
static const char kCafe[] = "850F7DC43910FF890F8879C0ED26FE697C93A067AD93A7D50F466A7028A9BF4E";
static const char kLoneHigh[] = "91A681B998555FB475479817B126C94E57E52011FA1842C5D188795A4A05226B";
static const char kLoneLow[] = "F826F8871B832A72ECD2CABF5458E98AC6BDA54107244400B69F6A0BA352F441";
static const char kPair[] = "76F25C09BAB91CFABA1EBBB8D6C8005090F9C45DB6895553BDEB2927501BFE76";
static const char kJapanese[] = "EE5B5C2106C60D499A10546BB389A6CBB258C595188AC2F49F3D8B50ACF52DA6";

static String Latin1(const std::string& text) {
    String result;
    _ZN3WTF6StringC1EPKhj(&result, reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
    return result;
}

static String Utf16(const std::u16string& text) {
    String result;
    _ZN3WTF6StringC1EPKDsj(&result, text.data(), static_cast<unsigned>(text.size()));
    return result;
}

static bool Is8Bit(const String& value) { return value.impl && (value.impl->hashAndFlags & 4); }

static std::string Text(const String& value) {
    Require(value.impl != nullptr, "result impl");
    Require(Is8Bit(value), "result is 8-bit");
    const auto* data = static_cast<const char*>(value.impl->data);
    return std::string(data, value.impl->length);
}

static std::string Hash(const String& fileName) {
    const std::size_t before = guestAllocationCount;
    std::string text;
    {
        String result = _ZN7WebCore16SQLiteFileSystem22computeHashForFileNameERKN3WTF6StringE(fileName);
        text = Text(result);
    }
    Require(guestAllocationCount == before + 1, "one digest object allocated");
    Require(guestAllocations.empty(), "digest object released");
    return text;
}

static void TestKnownVectors() {
    Require(Hash(Latin1("")) == kEmpty, "empty 8-bit");
    Require(Hash(Utf16(u"")) == kEmpty, "empty 16-bit");
    Require(Hash(Latin1("abc")) == kAbc, "abc 8-bit");
    Require(Hash(Utf16(u"abc")) == kAbc, "abc 16-bit");
}

static void TestNullString() {
    String null;
    Require(null.impl == nullptr, "null impl");
    Require(Hash(null) == kEmpty, "null string");
}

static void TestUtf8Encoding() {
    String latin1 = Latin1("caf\xE9");
    String wide = Utf16(u"café");
    Require(Is8Bit(latin1), "latin1 is 8-bit");
    Require(!Is8Bit(wide), "wide is 16-bit");
    Require(Hash(latin1) == kCafe, "latin1 utf-8");
    Require(Hash(wide) == kCafe, "16-bit utf-8");
    Require(Hash(Utf16(u"日本.db")) == kJapanese, "three-byte utf-8");
    Require(Hash(Utf16(u"a\U0001F600z")) == kPair, "surrogate pair");
}

static void TestLoneSurrogates() {
    const std::array<char16_t, 1> high{0xD800};
    Require(Hash(Utf16(std::u16string(high.begin(), high.end()))) == kLoneHigh, "lone high surrogate");
    const std::array<char16_t, 2> low{0xDC00, u'x'};
    Require(Hash(Utf16(std::u16string(low.begin(), low.end()))) == kLoneLow, "lone low surrogate");
}

static void TestResultShape() {
    String result = _ZN7WebCore16SQLiteFileSystem22computeHashForFileNameERKN3WTF6StringE(Latin1("database.db"));
    const std::string text = Text(result);
    Require(text.size() == 64, "64 characters");
    for (char c : text) Require((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'), "uppercase hex");
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestKnownVectors();
    TestNullString();
    TestUtf8Encoding();
    TestLoneSurrogates();
    TestResultShape();
    Require(guestAllocations.empty(), "all guest allocations released");
    return 0;
}
