#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SharedBufferViewExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using NKWebKit::WebCoreAbi::DataSegment;
using NKWebKit::WebCoreAbi::SharedBuffer;
using NKWebKit::WebCoreAbi::SharedBufferDataView;
using NKWebKit::WebCoreAbi::SharedBufferRef;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SharedBufferViews check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Exception, typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

static SharedBufferRef Build(std::initializer_list<std::string_view> parts) {
    auto buffer = _ZN7WebCore12SharedBuffer6createEv();
    for (auto part : parts) _ZN7WebCore12SharedBuffer6appendEPKcm(buffer.ptr, part.data(), part.size());
    return buffer;
}

static bool Equal(const SharedBuffer* a, const SharedBuffer* b) {
    const bool equal = _ZNK7WebCore12SharedBuffereqERKS0_(a, b);
    Require(_ZNK7WebCore12SharedBufferneERKS0_(a, b) == !equal);
    Require(_ZNK7WebCore12SharedBuffereqERKS0_(b, a) == equal);
    return equal;
}

static std::string_view View(const SharedBufferDataView& view) {
    return {_ZNK7WebCore20SharedBufferDataView4dataEv(&view), _ZNK7WebCore20SharedBufferDataView4sizeEv(&view)};
}

static std::string Text(const String& string) {
    Require(string.impl != nullptr);
    Require((string.impl->hashAndFlags & 4) != 0);
    return {static_cast<const char*>(string.impl->data), string.impl->length};
}

static String Utf8(const char* text) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(text));
}

static void TestEquality() {
    auto split = Build({"ab", "cdef"});
    auto whole = Build({"abcdef"});
    auto other = Build({"abc", "deg"});
    auto shorter = Build({"abcde"});
    auto empty = Build({});
    auto emptyToo = Build({});
    auto three = Build({"a", "bcd", "ef"});
    Require(Equal(split.ptr, split.ptr));
    Require(Equal(split.ptr, whole.ptr));
    Require(Equal(split.ptr, three.ptr));
    Require(!Equal(split.ptr, other.ptr));
    Require(!Equal(split.ptr, shorter.ptr));
    Require(Equal(empty.ptr, emptyToo.ptr));
    Require(!Equal(empty.ptr, whole.ptr));

    auto copy = _ZNK7WebCore12SharedBuffer4copyEv(split.ptr);
    Require(copy.ptr->segments.buffer[0].segment == split.ptr->segments.buffer[0].segment);
    Require(Equal(copy.ptr, split.ptr));
    _ZN7WebCore12SharedBuffer6appendEPKcm(copy.ptr, "g", 1);
    _ZN7WebCore12SharedBuffer6appendEPKcm(split.ptr, "h", 1);
    Require(!Equal(copy.ptr, split.ptr));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore12SharedBuffereqERKS0_(split.ptr, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore12SharedBufferneERKS0_(nullptr, split.ptr); }));
}

static void TestGetSomeData() {
    auto buffer = Build({"ab", "cde", "f"});
    const char* expected = "abcdef";
    const std::size_t segmentIndex[] = {0, 0, 1, 1, 1, 2};
    for (std::size_t position = 0; position < 6; ++position) {
        DataSegment* segment = buffer.ptr->segments.buffer[segmentIndex[position]].segment;
        const unsigned before = segment->refCount.load();
        {
            auto result = _ZNK7WebCore12SharedBuffer11getSomeDataEm(buffer.ptr, position);
            Require(result.view.segment == segment);
            Require(segment->refCount.load() == before + 1);
            Require(View(result.view).data() == _ZNK7WebCore12SharedBuffer11DataSegment4dataEv(segment) + result.view.positionWithinSegment);
            Require(View(result.view) == std::string_view(expected + position, _ZNK7WebCore12SharedBuffer11DataSegment4sizeEv(segment) - result.view.positionWithinSegment));
            Require(View(result.view).front() == expected[position]);
        }
        Require(segment->refCount.load() == before);
    }
    Require(Throws<std::out_of_range>([&] { _ZNK7WebCore12SharedBuffer11getSomeDataEm(buffer.ptr, 6); }));
    auto empty = Build({});
    Require(Throws<std::out_of_range>([&] { _ZNK7WebCore12SharedBuffer11getSomeDataEm(empty.ptr, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore12SharedBuffer11getSomeDataEm(nullptr, 0); }));
}

static void TestDataViewLifetime() {
    auto buffer = Build({"hello", "world"});
    DataSegment* first = buffer.ptr->segments.buffer[0].segment;
    DataSegment* second = buffer.ptr->segments.buffer[1].segment;
    Require(first->refCount.load() == 1 && second->refCount.load() == 1);

    first->refCount.fetch_add(1);
    DataSegment* reference = first;
    SharedBufferDataView view{};
    _ZN7WebCore20SharedBufferDataViewC1EON3WTF3RefINS_12SharedBuffer11DataSegmentENS1_13DumbPtrTraitsIS4_EEEEm(&view, &reference, 2);
    Require(!reference && view.segment == first && view.positionWithinSegment == 2);
    Require(first->refCount.load() == 2);
    Require(View(view) == "llo");

    SharedBufferDataView copy{};
    _ZN7WebCore20SharedBufferDataViewC1ERKS0_(&copy, &view);
    Require(copy.segment == first && copy.positionWithinSegment == 2 && first->refCount.load() == 3);
    SharedBufferDataView baseCopy{};
    _ZN7WebCore20SharedBufferDataViewC2ERKS0_(&baseCopy, &copy);
    Require(first->refCount.load() == 4);

    second->refCount.fetch_add(1);
    reference = second;
    SharedBufferDataView other{};
    _ZN7WebCore20SharedBufferDataViewC2EON3WTF3RefINS_12SharedBuffer11DataSegmentENS1_13DumbPtrTraitsIS4_EEEEm(&other, &reference, 4);
    Require(View(other) == "d" && second->refCount.load() == 2);

    Require(_ZN7WebCore20SharedBufferDataViewaSERKS0_(&copy, &other) == &copy);
    Require(copy.segment == second && copy.positionWithinSegment == 4);
    Require(first->refCount.load() == 3 && second->refCount.load() == 3);
    Require(_ZN7WebCore20SharedBufferDataViewaSERKS0_(&copy, &copy) == &copy);
    Require(second->refCount.load() == 3 && View(copy) == "d");

    _ZN7WebCore20SharedBufferDataViewD1Ev(&copy);
    _ZN7WebCore20SharedBufferDataViewD2Ev(&other);
    Require(second->refCount.load() == 1 && !copy.segment && !other.segment);
    _ZN7WebCore20SharedBufferDataViewD1Ev(&baseCopy);
    Require(first->refCount.load() == 2);

    auto survivor = _ZNK7WebCore12SharedBuffer11getSomeDataEm(buffer.ptr, 1);
    _ZN7WebCore12SharedBuffer5clearEv(buffer.ptr);
    Require(survivor.view.segment == first && first->refCount.load() == 2);
    Require(View(survivor.view) == "ello" && View(view) == "llo");
    _ZN7WebCore20SharedBufferDataViewD1Ev(&view);
    Require(first->refCount.load() == 1 && View(survivor.view) == "ello");

    DataSegment* none = nullptr;
    SharedBufferDataView invalid{};
    Require(Throws<std::invalid_argument>([&] {
        _ZN7WebCore20SharedBufferDataViewC1EON3WTF3RefINS_12SharedBuffer11DataSegmentENS1_13DumbPtrTraitsIS4_EEEEm(&invalid, &none, 0);
    }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore20SharedBufferDataViewC1ERKS0_(&invalid, &invalid); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore20SharedBufferDataView4sizeEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore20SharedBufferDataView4dataEv(&invalid); }));
}

static void TestHexString() {
    auto empty = Build({});
    {
        String hex = _ZNK7WebCore12SharedBuffer11toHexStringEv(empty.ptr);
        Require(Text(hex).empty());
    }
    const char bytes[] = {'\x00', '\xab', '\x7f', '\x10', '\xff'};
    auto buffer = Build({std::string_view(bytes, 2), std::string_view(bytes + 2, 3)});
    {
        String hex = _ZNK7WebCore12SharedBuffer11toHexStringEv(buffer.ptr);
        Require(Text(hex) == "00AB7F10FF");
    }
    Require(buffer.ptr->segments.size == 1 && _ZNK7WebCore12SharedBuffer4sizeEv(buffer.ptr) == 5);
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore12SharedBuffer11toHexStringEv(nullptr); }));

    _ZNK7WebCore12SharedBuffer23hintMemoryNotNeededSoonEv(buffer.ptr);
    Require(_ZNK7WebCore12SharedBuffer4sizeEv(buffer.ptr) == 5);
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore12SharedBuffer23hintMemoryNotNeededSoonEv(nullptr); }));
}

static std::string_view Contents(const SharedBuffer* buffer) {
    return {_ZNK7WebCore12SharedBuffer4dataEv(buffer), _ZNK7WebCore12SharedBuffer4sizeEv(buffer)};
}

static void TestFiles() {
    const auto root = std::filesystem::current_path() / ("nkwebkit-sharedbuffer-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-sharedbuffer", reinterpret_cast<const char*>(hostPath.c_str()));
    {
        std::ofstream file(root / "data.bin", std::ios::binary);
        file.write("hello\0world", 11);
    }
    { std::ofstream file(root / "empty.bin", std::ios::binary); }

    const String data = Utf8("/nkwebkit-sharedbuffer/data.bin");
    const String empty = Utf8("/nkwebkit-sharedbuffer/empty.bin");
    const String missing = Utf8("/nkwebkit-sharedbuffer/missing.bin");
    const String emptyPath = Utf8("");
    const String nullPath;

    {
        auto read = _ZN7WebCore12SharedBuffer21createFromReadingFileERKN3WTF6StringE(&data);
        Require(read.ptr && read.ptr->refCount == 1 && read.ptr->segments.size == 1);
        Require(read.ptr->segments.buffer[0].segment->index == 0);
        Require(Contents(read.ptr) == std::string_view("hello\0world", 11));

        auto mapped = _ZN7WebCore12SharedBuffer24createWithContentsOfFileERKN3WTF6StringE(&data);
        Require(mapped.ptr && mapped.ptr->refCount == 1 && mapped.ptr->segments.size == 1 && mapped.ptr->size == 11);
        const DataSegment* segment = mapped.ptr->segments.buffer[0].segment;
        Require(segment->index == 1 && segment->refCount.load() == 1 && segment->storage.mappedFile.size == 11);
        Require(Contents(mapped.ptr) == std::string_view("hello\0world", 11));
        Require(Equal(read.ptr, mapped.ptr));
        auto view = _ZNK7WebCore12SharedBuffer11getSomeDataEm(mapped.ptr, 6);
        Require(View(view.view) == "world");
    }
    {
        auto read = _ZN7WebCore12SharedBuffer21createFromReadingFileERKN3WTF6StringE(&empty);
        Require(read.ptr && read.ptr->size == 0 && read.ptr->segments.size == 1);
        auto mapped = _ZN7WebCore12SharedBuffer24createWithContentsOfFileERKN3WTF6StringE(&empty);
        Require(mapped.ptr && mapped.ptr->size == 0 && mapped.ptr->segments.size == 1);
        Require(mapped.ptr->segments.buffer[0].segment->index == 1);
        Require(Equal(read.ptr, mapped.ptr));
    }
    for (const String* path : {&missing, &emptyPath, &nullPath}) {
        Require(!_ZN7WebCore12SharedBuffer21createFromReadingFileERKN3WTF6StringE(path).ptr);
        Require(!_ZN7WebCore12SharedBuffer24createWithContentsOfFileERKN3WTF6StringE(path).ptr);
    }
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore12SharedBuffer21createFromReadingFileERKN3WTF6StringE(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore12SharedBuffer24createWithContentsOfFileERKN3WTF6StringE(nullptr); }));
    std::filesystem::remove_all(root);
}

int main() {
    TestEquality();
    TestGetSomeData();
    TestDataViewLifetime();
    TestHexString();
    TestFiles();
    return 0;
}
