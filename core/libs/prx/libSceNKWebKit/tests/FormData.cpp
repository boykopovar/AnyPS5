#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using NKWebKit::WebCoreAbi::FormData;
using NKWebKit::WebCoreAbi::FormDataElement;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, const char* name, int line) {
    if (!value) {
        std::fprintf(stderr, "FormData check failed for %s at line %d\n", name, line);
        std::abort();
    }
}
#define Require(value) Check((value), #value, __LINE__)

static std::map<void*, std::size_t> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations[pointer] = size;
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1);
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

template<class TException, class TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

static constexpr std::uint32_t kValueless = 0xFFFFFFFF;

static FormData* Create() {
    auto ref = _ZN7WebCore8FormData6createEv();
    return std::exchange(ref.m_ptr, nullptr);
}

static FormData* Create(const void* data, std::size_t size) {
    auto ref = _ZN7WebCore8FormData6createEPKvm(data, size);
    return std::exchange(ref.m_ptr, nullptr);
}

static void Free(FormData* formData) {
    _ZN7WebCore8FormDataD1Ev(formData);
    _ZdlPv_nid_postfix(formData);
}

static std::string Bytes(const FormDataElement& element) {
    return std::string(reinterpret_cast<const char*>(element.storage.bytes.buffer), element.storage.bytes.size);
}

static String MakeString(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static std::string Text(const StringImpl* impl) {
    return std::string(static_cast<const char*>(impl->data), impl->length);
}

static URL MakeURL(const String& string, unsigned seed) {
    URL url{};
    url.impl = string.impl;
    url.bits = seed;
    url.userStart = seed + 1;
    url.userEnd = seed + 2;
    url.passwordEnd = seed + 3;
    url.hostEnd = seed + 4;
    url.pathAfterLastSlash = seed + 5;
    url.pathEnd = seed + 6;
    url.queryEnd = seed + 7;
    return url;
}

static bool SameURL(const URL& a, const URL& b) {
    return a.impl == b.impl && a.bits == b.bits && a.userStart == b.userStart && a.userEnd == b.userEnd && a.passwordEnd == b.passwordEnd
        && a.hostEnd == b.hostEnd && a.pathAfterLastSlash == b.pathAfterLastSlash && a.pathEnd == b.pathEnd && a.queryEnd == b.queryEnd;
}

static bool IsEmptyURL(const URL& url) {
    static const URL zero{};
    return std::memcmp(&url, &zero, sizeof(URL)) == 0;
}

static FormDataElement& Push(FormData* formData) {
    auto& elements = formData->elements;
    if (elements.size == elements.capacity) {
        const unsigned capacity = elements.capacity ? elements.capacity * 2 : 4;
        auto* buffer = static_cast<FormDataElement*>(_ZN3WTF10fastMallocEm(capacity * sizeof(FormDataElement)));
        if (elements.size) std::memcpy(buffer, elements.buffer, elements.size * sizeof(FormDataElement));
        _ZN3WTF8fastFreeEPv(elements.buffer);
        elements.buffer = buffer;
        elements.capacity = capacity;
    }
    return elements.buffer[elements.size++];
}

static void PushFile(FormData* formData, const String& filename, std::int64_t start, std::int64_t length, bool hasTime = false, double time = 0) {
    auto& element = Push(formData);
    std::memset(&element, 0, sizeof(element));
    element.storage.file.filename = filename.impl;
    filename.impl->refCount += 2;
    element.storage.file.fileStart = start;
    element.storage.file.fileLength = length;
    element.storage.file.hasExpectedFileModificationTime = hasTime;
    element.storage.file.expectedFileModificationTime = time;
    element.index = 1;
}

static void PushBlob(FormData* formData, const URL& url) {
    auto& element = Push(formData);
    std::memset(&element, 0, sizeof(element));
    element.storage.blob.url = url;
    url.impl->refCount += 2;
    element.index = 2;
}

static void AppendBlob(FormData* formData, const String& string) {
    URL url = MakeURL(string, 0);
    _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(formData, &url);
}

static void PushValueless(FormData* formData, StringImpl* garbage) {
    auto& element = Push(formData);
    std::memset(&element, 0, sizeof(element));
    element.storage.file.filename = garbage;
    element.index = kValueless;
}

static void TestLayout() {
    FormData* formData = Create();
    Require(guestAllocations.size() == 1);
    Require(guestAllocations.begin()->first == formData);
    Require(guestAllocations.begin()->second == 72);
    Require(sizeof(FormDataElement) == 48);
    Require(offsetof(FormDataElement, index) == 40);
    Free(formData);
    Require(guestAllocations.empty());
}

static void TestCreate() {
    FormData* formData = Create();
    Require(formData->refCount == 1);
    Require(formData->elements.buffer == nullptr);
    Require(formData->elements.capacity == 0);
    Require(formData->elements.size == 0);
    Require(formData->identifier == 0);
    Require(!formData->alwaysStream);
    Require(formData->boundary.buffer == nullptr);
    Require(formData->boundary.capacity == 0);
    Require(formData->boundary.size == 0);
    Require(!formData->hasLengthInBytes);
    Free(formData);

    formData = Create("abc", 3);
    Require(formData->refCount == 1);
    Require(formData->elements.size == 1);
    Require(formData->elements.capacity == 16);
    Require(formData->elements.buffer[0].index == 0);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 3);
    Require(Bytes(formData->elements.buffer[0]) == "abc");
    Require(formData->identifier == 0);
    Require(formData->boundary.buffer == nullptr);
    Require(!formData->hasLengthInBytes);
    Free(formData);

    formData = Create(nullptr, 0);
    Require(formData->elements.size == 1);
    Require(formData->elements.buffer[0].index == 0);
    Require(formData->elements.buffer[0].storage.bytes.buffer == nullptr);
    Require(formData->elements.buffer[0].storage.bytes.size == 0);
    Free(formData);

    Require(Throws<std::invalid_argument>([] { Create(nullptr, 1); }));
    Require(guestAllocations.empty());
}

static void TestRefCounting() {
    auto ref = _ZN7WebCore8FormData6createEv();
    ref.m_ptr->ref();
    Require(ref.m_ptr->refCount == 2);
    ref.m_ptr->deref();
    Require(ref.m_ptr->refCount == 1);
    Require(guestAllocations.size() == 1);
    {
        auto copy = ref;
        Require(ref.m_ptr->refCount == 2);
    }
    Require(ref.m_ptr->refCount == 1);
    ref = WebCore::Ref<FormData>(*Create());
    Require(guestAllocations.size() == 1);
    ref = WebCore::Ref<FormData>(*Create());
    Require(guestAllocations.size() == 1);
    ref.m_ptr->deref();
    ref.m_ptr = nullptr;
    Require(guestAllocations.empty());
}

static void TestAppendData() {
    FormData* formData = Create();
    formData->hasLengthInBytes = true;
    formData->lengthInBytes = 99;
    _ZN7WebCore8FormData10appendDataEPKvm(formData, "a", 1);
    Require(!formData->hasLengthInBytes);
    Require(formData->elements.size == 1);
    Require(formData->elements.capacity == 16);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 1);

    _ZN7WebCore8FormData10appendDataEPKvm(formData, "b", 1);
    Require(formData->elements.size == 1);
    Require(formData->elements.buffer[0].storage.bytes.size == 2);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 16);
    Require(Bytes(formData->elements.buffer[0]) == "ab");

    const std::string fifteen(15, 'c');
    _ZN7WebCore8FormData10appendDataEPKvm(formData, fifteen.data(), fifteen.size());
    Require(formData->elements.buffer[0].storage.bytes.size == 17);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 21);

    const std::string hundred(100, 'd');
    _ZN7WebCore8FormData10appendDataEPKvm(formData, hundred.data(), hundred.size());
    Require(formData->elements.buffer[0].storage.bytes.size == 117);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 117);

    _ZN7WebCore8FormData10appendDataEPKvm(formData, "e", 1);
    Require(formData->elements.buffer[0].storage.bytes.size == 118);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 147);
    Require(Bytes(formData->elements.buffer[0]) == "ab" + fifteen + hundred + "e");

    _ZN7WebCore8FormData10appendDataEPKvm(formData, nullptr, 0);
    Require(formData->elements.size == 1);
    Require(formData->elements.buffer[0].storage.bytes.size == 118);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 147);
    Free(formData);

    formData = Create();
    _ZN7WebCore8FormData10appendDataEPKvm(formData, nullptr, 0);
    Require(formData->elements.size == 1);
    Require(formData->elements.buffer[0].index == 0);
    Require(formData->elements.buffer[0].storage.bytes.capacity == 0);
    _ZN7WebCore8FormData10appendDataEPKvm(formData, nullptr, 0);
    Require(formData->elements.size == 1);
    Free(formData);

    String name = MakeString("blob:a");
    formData = Create("x", 1);
    PushBlob(formData, MakeURL(name, 1));
    _ZN7WebCore8FormData10appendDataEPKvm(formData, "y", 1);
    Require(formData->elements.size == 3);
    Require(formData->elements.buffer[2].index == 0);
    Require(Bytes(formData->elements.buffer[0]) == "x");
    Require(Bytes(formData->elements.buffer[2]) == "y");
    Require(formData->elements.buffer[2].storage.bytes.capacity == 1);
    _ZN7WebCore8FormData10appendDataEPKvm(formData, nullptr, 0);
    Require(formData->elements.size == 3);
    Free(formData);
    Require(name.impl->refCount == 2);

    formData = Create();
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore8FormData10appendDataEPKvm(formData, nullptr, 1); }));
    Require(formData->elements.size == 0);
    Free(formData);
    Require(guestAllocations.empty());
}

static void TestElementGrowth() {
    String name = MakeString("blob:g");
    URL url = MakeURL(name, 0);
    FormData* formData = Create();
    const std::vector<std::pair<unsigned, unsigned>> expected = {{1, 16}, {16, 16}, {17, 21}, {21, 21}, {22, 27}, {27, 27}, {28, 34}};
    unsigned count = 0;
    for (auto [size, capacity] : expected) {
        while (count < size) {
            _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(formData, &url);
            ++count;
        }
        Require(formData->elements.size == size);
        Require(formData->elements.capacity == capacity);
    }
    Require(name.impl->refCount == 2 + 2 * 28);
    Free(formData);
    Require(name.impl->refCount == 2);
    Require(guestAllocations.empty());
}

static void TestAppendBlob() {
    String name = MakeString("blob:b");
    URL url = MakeURL(name, 10);
    FormData* formData = Create("ab", 2);
    Require(_ZNK7WebCore8FormData13lengthInBytesEv(formData) == 2);
    Require(formData->hasLengthInBytes);
    _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(formData, &url);
    Require(!formData->hasLengthInBytes);
    Require(name.impl->refCount == 4);
    Require(formData->elements.size == 2);
    const auto& element = formData->elements.buffer[1];
    Require(element.index == 2);
    Require(SameURL(element.storage.blob.url, url));
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore8FormData13lengthInBytesEv(formData); }));
    Require(!formData->hasLengthInBytes);
    Free(formData);
    Require(name.impl->refCount == 2);

    formData = Create();
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(formData, nullptr); }));
    Require(formData->elements.size == 0);
    Free(formData);
    Require(guestAllocations.empty());
}

static void TestAppendBlobFromOwnElement() {
    String name = MakeString("blob:self");
    FormData* formData = Create();
    for (unsigned i = 0; i < 16; ++i) AppendBlob(formData, name);
    Require(formData->elements.size == 16);
    Require(formData->elements.capacity == 16);
    StringImpl* impl = name.impl;
    const unsigned before = impl->refCount;
    _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(formData, &formData->elements.buffer[0].storage.blob.url);
    Require(formData->elements.size == 17);
    Require(formData->elements.capacity > 16);
    Require(formData->elements.buffer[16].index == 2);
    Require(formData->elements.buffer[16].storage.blob.url.impl == impl);
    Require(impl->refCount == before + 2);
    Free(formData);
    Require(name.impl->refCount == 2);
    Require(guestAllocations.empty());
}

static void TestFlatten() {
    String name = MakeString("blob:f");
    String file = MakeString("/tmp/f");
    FormData* formData = Create();
    {
        auto empty = _ZNK7WebCore8FormData7flattenEv(formData);
        Require(empty.buffer == nullptr);
        Require(empty.capacity == 0);
        Require(empty.size == 0);
    }
    _ZN7WebCore8FormData10appendDataEPKvm(formData, "abc", 3);
    PushFile(formData, file, 0, 4);
    AppendBlob(formData, name);
    _ZN7WebCore8FormData10appendDataEPKvm(formData, "de", 2);
    _ZN7WebCore8FormData10appendDataEPKvm(formData, nullptr, 0);
    _ZN7WebCore8FormData10appendDataEPKvm(formData, "f", 1);
    {
        auto flat = _ZNK7WebCore8FormData7flattenEv(formData);
        Require(flat.size == 6);
        Require(flat.capacity == 16);
        Require(std::memcmp(flat.buffer, "abcdef", 6) == 0);
    }
    PushFile(formData, file, 0, 1);
    formData->elements.buffer[0].storage.bytes.size = 0;
    {
        auto flat = _ZNK7WebCore8FormData7flattenEv(formData);
        Require(flat.size == 3);
        Require(flat.capacity == 16);
    }
    formData->elements.buffer[0].storage.bytes.size = 3;
    const std::string large(40, 'z');
    _ZN7WebCore8FormData10appendDataEPKvm(formData, large.data(), large.size());
    {
        auto flat = _ZNK7WebCore8FormData7flattenEv(formData);
        Require(flat.size == 46);
        Require(flat.capacity == 46);
        Require(std::memcmp(flat.buffer, "abcdef", 6) == 0);
        Require(std::memcmp(flat.buffer + 6, large.data(), 40) == 0);
    }
    Free(formData);

    formData = Create();
    PushValueless(formData, nullptr);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore8FormData7flattenEv(formData); }));
    Free(formData);
    Require(guestAllocations.empty());
}

static void TestAsBlobURL() {
    String name = MakeString("blob:u");
    URL url = MakeURL(name, 20);
    FormData* formData = Create();
    Require(IsEmptyURL(_ZNK7WebCore8FormData9asBlobURLEv(formData)));
    PushBlob(formData, url);
    Require(name.impl->refCount == 4);
    {
        URL result = _ZNK7WebCore8FormData9asBlobURLEv(formData);
        Require(SameURL(result, url));
        Require(name.impl->refCount == 6);
        String release(result.impl);
    }
    Require(name.impl->refCount == 4);
    PushBlob(formData, url);
    Require(IsEmptyURL(_ZNK7WebCore8FormData9asBlobURLEv(formData)));
    Free(formData);
    Require(name.impl->refCount == 2);

    formData = Create("abc", 3);
    Require(IsEmptyURL(_ZNK7WebCore8FormData9asBlobURLEv(formData)));
    Require(name.impl->refCount == 2);
    Free(formData);

    formData = Create();
    String file = MakeString("/tmp/u");
    PushFile(formData, file, 0, 1);
    Require(IsEmptyURL(_ZNK7WebCore8FormData9asBlobURLEv(formData)));
    Free(formData);
    Require(guestAllocations.empty());
}

static void TestLengthInBytes() {
    String file = MakeString("/tmp/l");
    String name = MakeString("blob:l");
    FormData* formData = Create();
    Require(_ZNK7WebCore8FormData13lengthInBytesEv(formData) == 0);
    Require(formData->hasLengthInBytes);
    _ZN7WebCore8FormData10appendDataEPKvm(formData, "abc", 3);
    Require(!formData->hasLengthInBytes);
    PushFile(formData, file, 5, 100);
    PushFile(formData, file, 0, 0);
    _ZN7WebCore8FormData10appendDataEPKvm(formData, "x", 1);
    Require(_ZNK7WebCore8FormData13lengthInBytesEv(formData) == 104);
    Require(formData->hasLengthInBytes);
    Require(formData->lengthInBytes == 104);
    formData->elements.buffer[0].storage.bytes.size = 1;
    PushFile(formData, file, 0, 1000);
    Require(_ZNK7WebCore8FormData13lengthInBytesEv(formData) == 104);
    formData->hasLengthInBytes = false;
    Require(_ZNK7WebCore8FormData13lengthInBytesEv(formData) == 1102);
    formData->hasLengthInBytes = false;
    Free(formData);

    formData = Create("abc", 3);
    PushFile(formData, file, 0, -1);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore8FormData13lengthInBytesEv(formData); }));
    Require(!formData->hasLengthInBytes);
    Free(formData);

    formData = Create("abc", 3);
    AppendBlob(formData, name);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore8FormData13lengthInBytesEv(formData); }));
    Require(!formData->hasLengthInBytes);
    Free(formData);

    formData = Create();
    PushValueless(formData, nullptr);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore8FormData13lengthInBytesEv(formData); }));
    Require(!formData->hasLengthInBytes);
    Free(formData);
    Require(guestAllocations.empty());
}

static void TestIsolatedCopy() {
    String file = MakeString("/tmp/file.bin");
    String name = MakeString("blob:isolated");
    FormData* source = Create("ab", 2);
    _ZN7WebCore8FormData10appendDataEPKvm(source, "cd", 2);
    PushFile(source, file, 7, 9, true, 12.5);
    PushFile(source, file, 1, -1);
    URL url = MakeURL(name, 30);
    _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(source, &url);
    _ZN7WebCore8FormData10appendDataEPKvm(source, nullptr, 0);
    source->alwaysStream = true;
    source->identifier = 77;
    source->boundary.buffer = static_cast<std::uint8_t*>(_ZN3WTF10fastMallocEm(4));
    std::memcpy(source->boundary.buffer, "abcd", 4);
    source->boundary.capacity = 4;
    source->boundary.size = 4;
    source->hasLengthInBytes = true;
    source->lengthInBytes = 5;
    const unsigned fileRefs = file.impl->refCount;
    const unsigned nameRefs = name.impl->refCount;

    auto ref = _ZNK7WebCore8FormData12isolatedCopyEv(source);
    FormData* copy = std::exchange(ref.m_ptr, nullptr);
    Require(copy != source);
    Require(copy->refCount == 1);
    Require(copy->alwaysStream);
    Require(copy->identifier == 0);
    Require(copy->boundary.buffer == nullptr);
    Require(copy->boundary.size == 0);
    Require(!copy->hasLengthInBytes);
    Require(copy->elements.size == 5);
    Require(copy->elements.capacity == 5);
    Require(file.impl->refCount == fileRefs);
    Require(name.impl->refCount == nameRefs);

    const auto& a = copy->elements.buffer[0];
    Require(a.index == 0);
    Require(Bytes(a) == "abcd");
    Require(a.storage.bytes.capacity == 4);
    Require(a.storage.bytes.buffer != source->elements.buffer[0].storage.bytes.buffer);

    const auto& f = copy->elements.buffer[1];
    Require(f.index == 1);
    Require(f.storage.file.filename != file.impl);
    Require(Text(f.storage.file.filename) == "/tmp/file.bin");
    Require(f.storage.file.filename->refCount == 2);
    Require(f.storage.file.fileStart == 7);
    Require(f.storage.file.fileLength == 9);
    Require(f.storage.file.hasExpectedFileModificationTime);
    Require(f.storage.file.expectedFileModificationTime == 12.5);

    const auto& g = copy->elements.buffer[2];
    Require(g.index == 1);
    Require(g.storage.file.fileStart == 1);
    Require(g.storage.file.fileLength == -1);
    Require(!g.storage.file.hasExpectedFileModificationTime);

    const auto& b = copy->elements.buffer[3];
    Require(b.index == 2);
    Require(b.storage.blob.url.impl != name.impl);
    Require(Text(b.storage.blob.url.impl) == "blob:isolated");
    Require(b.storage.blob.url.bits == url.bits);
    Require(b.storage.blob.url.userStart == url.userStart);
    Require(b.storage.blob.url.userEnd == url.userEnd);
    Require(b.storage.blob.url.passwordEnd == url.passwordEnd);
    Require(b.storage.blob.url.hostEnd == url.hostEnd);
    Require(b.storage.blob.url.pathAfterLastSlash == url.pathAfterLastSlash);
    Require(b.storage.blob.url.pathEnd == url.pathEnd);
    Require(b.storage.blob.url.queryEnd == url.queryEnd);

    const auto& e = copy->elements.buffer[4];
    Require(e.index == 0);
    Require(e.storage.bytes.buffer == nullptr);
    Require(e.storage.bytes.capacity == 0);

    Require(source->hasLengthInBytes);
    Require(source->identifier == 77);
    Require(source->boundary.size == 4);
    Free(copy);
    Free(source);
    Require(file.impl->refCount == 2);
    Require(name.impl->refCount == 2);

    source = Create();
    ref = _ZNK7WebCore8FormData12isolatedCopyEv(source);
    Require(ref.m_ptr->elements.buffer == nullptr);
    Require(ref.m_ptr->elements.capacity == 0);
    Require(!ref.m_ptr->alwaysStream);
    ref.m_ptr->deref();
    ref.m_ptr = nullptr;
    Free(source);

    source = Create();
    PushValueless(source, nullptr);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore8FormData12isolatedCopyEv(source); }));
    Free(source);
    Require(guestAllocations.size() == 0);

    source = Create("abc", 3);
    PushFile(source, file, 0, 1);
    source->elements.buffer[1].storage.file.filename = nullptr;
    file.impl->refCount -= 2;
    ref = _ZNK7WebCore8FormData12isolatedCopyEv(source);
    Require(ref.m_ptr->elements.buffer[1].storage.file.filename == nullptr);
    ref.m_ptr->deref();
    ref.m_ptr = nullptr;
    Free(source);
    Require(guestAllocations.empty());
}

static void TestIsolatedCopyFailure() {
    String file = MakeString("/tmp/failure");
    FormData* source = Create("abc", 3);
    PushFile(source, file, 0, 1);
    PushValueless(source, nullptr);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore8FormData12isolatedCopyEv(source); }));
    Require(guestAllocations.size() == 1);
    Free(source);
    Require(file.impl->refCount == 2);
    Require(guestAllocations.empty());
}

static void TestDestructor() {
    String file = MakeString("/tmp/d");
    String name = MakeString("blob:d");
    FormData* formData = Create("abc", 3);
    PushFile(formData, file, 0, 1);
    AppendBlob(formData, name);
    formData->boundary.buffer = static_cast<std::uint8_t*>(_ZN3WTF10fastMallocEm(2));
    formData->boundary.capacity = formData->boundary.size = 2;
    Require(file.impl->refCount == 4);
    Require(name.impl->refCount == 4);
    _ZN7WebCore8FormDataD1Ev(formData);
    Require(file.impl->refCount == 2);
    Require(name.impl->refCount == 2);
    Require(formData->elements.buffer == nullptr);
    Require(formData->elements.size == 0);
    Require(formData->boundary.buffer == nullptr);
    _ZdlPv_nid_postfix(formData);
    Require(guestAllocations.empty());

    formData = Create("abc", 3);
    PushFile(formData, file, 0, 1);
    _ZN7WebCore8FormDataD2Ev(formData);
    Require(file.impl->refCount == 2);
    _ZdlPv_nid_postfix(formData);
    Require(guestAllocations.empty());

    formData = Create();
    _ZN7WebCore8FormDataD1Ev(formData);
    _ZdlPv_nid_postfix(formData);
    Require(guestAllocations.empty());
}

static void TestDestructorSharedString() {
    String shared = MakeString("/tmp/shared");
    FormData* formData = Create();
    PushFile(formData, shared, 0, 1);
    PushBlob(formData, MakeURL(shared, 0));
    PushFile(formData, shared, 0, 2);
    Require(shared.impl->refCount == 8);
    Free(formData);
    Require(shared.impl->refCount == 2);

    formData = Create();
    PushFile(formData, shared, 0, 1);
    PushBlob(formData, MakeURL(shared, 0));
    StringImpl* impl = std::exchange(shared.impl, nullptr);
    impl->refCount -= 2;
    Require(impl->refCount == 4);
    Free(formData);
    Require(guestAllocations.empty());
}

static void TestDestructorSubstringOwner() {
    String owner = MakeString("/tmp/owner");
    StringImpl* ownerImpl = owner.impl;
    ownerImpl->refCount += 2;
    auto* raw = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(32));
    std::memset(raw, 0, 32);
    auto* substring = reinterpret_cast<StringImpl*>(raw);
    *substring = StringImpl{2, 3, ownerImpl->data, 4 | 2};
    *reinterpret_cast<StringImpl**>(raw + 24) = ownerImpl;
    String view(substring);
    FormData* formData = Create();
    PushFile(formData, view, 0, 1);
    view.impl = nullptr;
    substring->refCount -= 2;
    Require(substring->refCount == 2);
    Require(ownerImpl->refCount == 4);
    Free(formData);
    Require(ownerImpl->refCount == 2);
    Require(guestAllocations.empty());
}

static void TestDestructorValueless() {
    String untouched = MakeString("/tmp/valueless");
    FormData* formData = Create("abc", 3);
    PushValueless(formData, untouched.impl);
    PushFile(formData, untouched, 0, 1);
    Require(untouched.impl->refCount == 4);
    Free(formData);
    Require(untouched.impl->refCount == 2);
    Require(guestAllocations.empty());

    formData = Create("abc", 3);
    PushFile(formData, untouched, 0, 1);
    FormDataElement* buffer = formData->elements.buffer;
    const unsigned size = formData->elements.size;
    const unsigned capacity = formData->elements.capacity;
    std::uint8_t* bytes = buffer[0].storage.bytes.buffer;
    buffer[1].index = 3;
    Require(Throws<std::logic_error>([&] { _ZN7WebCore8FormDataD1Ev(formData); }));
    Require(formData->elements.buffer == buffer);
    Require(formData->elements.size == size);
    Require(formData->elements.capacity == capacity);
    Require(buffer[0].storage.bytes.buffer == bytes);
    Require(Bytes(buffer[0]) == "abc");
    Require(buffer[1].storage.file.filename == untouched.impl);
    Require(untouched.impl->refCount == 4);
    buffer[1].index = 1;
    Free(formData);
    Require(untouched.impl->refCount == 2);
    Require(guestAllocations.empty());
}

static void TestNullPointers() {
    FormData* formData = Create();
    String name = MakeString("blob:n");
    URL url = MakeURL(name, 0);
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore8FormData6createEPKvm(nullptr, 1); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore8FormData10appendDataEPKvm(nullptr, "a", 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore8FormData10appendDataEPKvm(formData, nullptr, 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(nullptr, &url); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(formData, nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore8FormDataD1Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore8FormDataD2Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore8FormData12isolatedCopyEv(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore8FormData7flattenEv(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore8FormData9asBlobURLEv(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore8FormData13lengthInBytesEv(nullptr); }));
    Require(formData->elements.size == 0);
    Require(name.impl->refCount == 2);
    Free(formData);
    Require(guestAllocations.empty());
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestLayout();
    TestCreate();
    TestRefCounting();
    TestAppendData();
    TestElementGrowth();
    TestAppendBlob();
    TestAppendBlobFromOwnElement();
    TestFlatten();
    TestAsBlobURL();
    TestLengthInBytes();
    TestIsolatedCopy();
    TestIsolatedCopyFailure();
    TestDestructor();
    TestDestructorSharedString();
    TestDestructorSubstringOwner();
    TestDestructorValueless();
    TestNullPointers();
    Require(guestAllocations.empty());
    return 0;
}
