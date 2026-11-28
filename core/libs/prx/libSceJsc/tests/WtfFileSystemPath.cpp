#include "prx/libSceJsc/include/WtfFileSystemExports.hpp"
#include "prx/libSceJsc/include/WtfSha1Exports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <stdexcept>
#include <string>

using namespace Jsc::WtfAbi;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF::FileSystemImpl::pathByAppendingComponent check failed at line %d\n", line);
        std::exit(1);
    }
}

std::set<void*> live;
std::size_t lastSize = 0;
bool failNext = false;

void* TestAllocate(std::size_t size) {
    lastSize = size;
    if (failNext) {
        failNext = false;
        return nullptr;
    }
    void* storage = std::malloc(size);
    live.insert(storage);
    return storage;
}

void TestDeallocate(void* storage) {
    Check(live.erase(storage) == 1, __LINE__);
    std::free(storage);
}

const PersistenceAllocator allocator{&TestAllocate, &TestDeallocate};

constexpr std::size_t CharactersOffset = 20;

struct Text {
    StringImpl impl;
    String string;
};

Text* Make8(const char* text) {
    auto* value = new Text{{2, static_cast<unsigned>(std::strlen(text)), text, StringImplFlag8Bit}, {}};
    value->string.impl = &value->impl;
    return value;
}

Text* Make16(const std::u16string& text) {
    auto* value = new Text{{2, static_cast<unsigned>(text.size()), text.data(), 0}, {}};
    value->string.impl = &value->impl;
    return value;
}

StringImpl* Append(const Text* path, const Text* component) {
    StringImpl* result = nullptr;
    StringImpl** returned = _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(&result, &path->string, &component->string);
    Check(returned == &result && result, __LINE__);
    Check(result->refCount == 2 && result->data == reinterpret_cast<const char*>(result) + CharactersOffset, __LINE__);
    return result;
}

bool Is8(const StringImpl* string, const char* text) {
    const unsigned length = static_cast<unsigned>(std::strlen(text));
    return string->length == length && string->hashAndFlags == StringImplFlag8Bit && lastSize == CharactersOffset + length &&
        std::memcmp(string->data, text, length) == 0;
}

bool Is16(const StringImpl* string, const std::u16string& text) {
    return string->length == text.size() && string->hashAndFlags == 0 && lastSize == CharactersOffset + text.size() * 2 &&
        std::memcmp(string->data, text.data(), text.size() * 2) == 0;
}

template<typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

void Release(StringImpl* string) {
    TestDeallocate(string);
}

}

int main() {
    RegisterPersistenceAllocator_nid_no_patch(&allocator);

    const struct {
        const char* path;
        const char* component;
        const char* expected;
    } cases[] = {
        {"/app0", "data.bin", "/app0/data.bin"},
        {"/app0/", "data.bin", "/app0/data.bin"},
        {"/app0//", "x", "/app0//x"},
        {"/", "x", "/x"},
        {"", "x", "/x"},
        {"", "", "/"},
        {"/a", "", "/a/"},
        {"/a/", "", "/a/"},
        {"a", "/b", "a//b"},
        {"a/", "/b", "a//b"},
        {"C:\\dir", "f", "C:\\dir/f"},
    };
    for (const auto& item : cases) {
        Text* path = Make8(item.path);
        Text* component = Make8(item.component);
        StringImpl* result = Append(path, component);
        Check(Is8(result, item.expected), __LINE__);
        Release(result);
        delete path;
        delete component;
    }

    {
        Text nullString{};
        Text* component = Make8("x");
        StringImpl* result = Append(&nullString, component);
        Check(Is8(result, "/x"), __LINE__);
        Release(result);
        Text* path = Make8("/a/");
        result = Append(path, &nullString);
        Check(Is8(result, "/a/"), __LINE__);
        Release(result);
        result = Append(&nullString, &nullString);
        Check(Is8(result, "/"), __LINE__);
        Release(result);
        delete component;
        delete path;
    }

    {
        const std::u16string wide = u"\u00e9\u0100";
        Text* path = Make8("/app0");
        Text* component = Make16(wide);
        StringImpl* result = Append(path, component);
        Check(Is16(result, u"/app0/\u00e9\u0100"), __LINE__);
        Release(result);

        const std::u16string widePath = u"/\u0100/";
        Text* path16 = Make16(widePath);
        Text* plain = Make8("f\xe9");
        result = Append(path16, plain);
        Check(Is16(result, u"/\u0100/f\u00e9"), __LINE__);
        Release(result);

        const std::u16string slashOnly = u"/x\u2215";
        Text* fakeSlash = Make16(slashOnly);
        result = Append(fakeSlash, plain);
        Check(Is16(result, u"/x\u2215/f\u00e9"), __LINE__);
        Release(result);

        Text* empty16 = Make16(u"");
        result = Append(empty16, plain);
        Check(Is16(result, u"/f\u00e9"), __LINE__);
        Release(result);

        Text* plain2 = Make8("\xe9");
        result = Append(plain2, plain);
        Check(Is8(result, "\xe9/f\xe9"), __LINE__);
        Release(result);
        delete path;
        delete component;
        delete path16;
        delete plain;
        delete fakeSlash;
        delete empty16;
        delete plain2;
    }

    {
        Text* path = Make8("/a");
        StringImpl* result = reinterpret_cast<StringImpl*>(0x1234);
        Check(Throws<std::invalid_argument>([&] {
            _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(&result, nullptr, &path->string);
        }), __LINE__);
        Check(Throws<std::invalid_argument>([&] {
            _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(&result, &path->string, nullptr);
        }), __LINE__);
        Check(Throws<std::invalid_argument>([&] {
            _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(nullptr, &path->string, &path->string);
        }), __LINE__);
        Check(live.empty(), __LINE__);

        Text missing{{2, 3, nullptr, StringImplFlag8Bit}, {}};
        missing.string.impl = &missing.impl;
        Check(Throws<std::invalid_argument>([&] {
            _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(&result, &missing.string, &path->string);
        }), __LINE__);

        Text huge{{2, static_cast<unsigned>(std::numeric_limits<int>::max()), "", StringImplFlag8Bit}, {}};
        huge.string.impl = &huge.impl;
        Text emptyText{{2, 0, "", StringImplFlag8Bit}, {}};
        emptyText.string.impl = &emptyText.impl;
        Check(Throws<std::length_error>([&] {
            _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(&result, &emptyText.string, &huge.string);
        }), __LINE__);

        failNext = true;
        Check(Throws<std::bad_alloc>([&] {
            _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(&result, &path->string, &path->string);
        }), __LINE__);

        RegisterPersistenceAllocator_nid_no_patch(nullptr);
        Check(Throws<std::runtime_error>([&] {
            _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(&result, &path->string, &path->string);
        }), __LINE__);
        Check(result == reinterpret_cast<StringImpl*>(0x1234), __LINE__);
        delete path;
    }

    Check(live.empty(), __LINE__);
    std::puts("jsc_wtf_file_system_path: all checks passed");
    return 0;
}
