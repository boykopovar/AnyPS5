#include "prx/libSceXml/include/Api.hpp"
#include "prx/libSceXml/include/InventoryExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

struct TestAllocator {
    XmlMemAllocator base;
    std::vector<void*> blocks;
    std::string log;
    std::size_t allocations = 0;
    std::size_t failAt = 0;
    std::size_t first = 0;
};

void* APS5_VABI TestAllocate(XmlMemAllocator* self, std::size_t size, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "allocate receives the user data");
    ++allocator->allocations;
    allocator->log += "a" + std::to_string(size) + " ";
    if (allocator->allocations == allocator->failAt) return nullptr;
    void* block = std::malloc(size);
    allocator->blocks.push_back(block);
    return block;
}

void APS5_VABI TestDeallocate(XmlMemAllocator* self, void* pointer, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "deallocate receives the user data");
    std::size_t index = 0;
    while (index < allocator->blocks.size() && allocator->blocks[index] != pointer) ++index;
    Require(index < allocator->blocks.size(), "deallocate receives a live block");
    allocator->log += "f" + std::to_string(index - allocator->first) + " ";
    std::free(pointer);
    allocator->blocks[index] = nullptr;
}

void APS5_VABI TestDestroy(XmlMemAllocator*) {}

const XmlMemAllocatorVtable g_testVtable{TestDestroy, TestDestroy, TestAllocate, TestDeallocate};

struct Fixture {
    TestAllocator allocator{};
    XmlInitParameter parameter{};
    XmlHandle initializer{nullptr};

    Fixture() {
        allocator.base.vtable = &g_testVtable;
        parameter = {&allocator.base, reinterpret_cast<void*>(0x1234), {0, 0}};
        Require(_ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0, "the initializer starts");
    }

    ~Fixture() {
        _ZN3sce3Xml11Initializer9terminateEv(&initializer);
        for (void* block : allocator.blocks) {
            if (block != nullptr) std::free(block);
        }
    }

    void Mark() {
        allocator.log.clear();
        allocator.first = allocator.blocks.size();
    }

    std::string Take() {
        std::string log = std::move(allocator.log);
        allocator.log.clear();
        return log;
    }

    void FailNext(std::size_t count) {
        allocator.failAt = allocator.allocations + count;
    }
};

XmlString Text(const char* text) {
    return XmlString{text, text != nullptr ? std::strlen(text) : 0};
}

std::string Name(const XmlHandle* attr) {
    XmlString result{reinterpret_cast<const char*>(1), 7};
    _ZNK3sce3Xml4Attr7getNameEv(&result, attr);
    return result.buffer != nullptr ? std::string(result.buffer, result.size) : std::string("<null>");
}

std::string Value(const XmlHandle* attr) {
    XmlString result{reinterpret_cast<const char*>(1), 7};
    _ZNK3sce3Xml4Attr8getValueEv(&result, attr);
    return result.buffer != nullptr ? std::string(result.buffer, result.size) : std::string("<null>");
}

int Add(XmlHandle* list, const XmlString& name, const XmlString& value) {
    return _ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(list, &name, &value);
}

int Add(XmlHandle* list, const char* name, const char* value) {
    return Add(list, Text(name), Text(value));
}

std::string ByName(const XmlHandle* list, const XmlString& name) {
    XmlHandle attr{reinterpret_cast<void*>(0x5555)};
    Require(_ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&attr, list, &name) == &attr, "getAttribute returns its result");
    if (attr.impl == nullptr) return "-";
    const std::string text = Name(&attr) + "=" + Value(&attr);
    _ZN3sce3Xml4AttrD1Ev(&attr);
    return text;
}

std::string ByIndex(const XmlHandle* list, std::uint32_t index) {
    XmlHandle attr{reinterpret_cast<void*>(0x5555)};
    Require(_ZNK3sce3Xml13AttributeList12getAttributeEj(&attr, list, index) == &attr, "getAttribute returns its result");
    if (attr.impl == nullptr) return "-";
    const std::string text = Name(&attr) + "=" + Value(&attr);
    _ZN3sce3Xml4AttrD1Ev(&attr);
    return text;
}

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

const int NotInitialized = static_cast<int>(0x80850002);
const int NoMemory = static_cast<int>(0x80850001);
const int InvalidParameter = static_cast<int>(0x80850003);

void TestListLifecycle() {
    Fixture f;
    f.Mark();
    XmlHandle list{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml13AttributeListC1Ev(&list);
    Require(list.impl == nullptr, "the constructor clears the object");
    Require(!_ZNK3sce3Xml13AttributeList11isAvailableEv(&list), "an uninitialized list is not available");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 0, "an uninitialized list has no attributes");
    Require(_ZN3sce3Xml13AttributeList9terminateEv(&list) == 0, "terminate before initialize succeeds");
    Require(Add(&list, "x", "1") == NotInitialized, "addAttribute before initialize");
    _ZN3sce3Xml13AttributeList5clearEv(&list);

    XmlHandle never{nullptr};
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, nullptr) == NoMemory, "a null initializer");
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &never) == NoMemory, "an initializer without an implementation");
    Require(list.impl == nullptr && f.Take().empty(), "a refused initialize allocates nothing");

    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    Require(f.Take() == "a32 a40 ", "initialize allocates the object and the attribute table");
    Require(_ZNK3sce3Xml13AttributeList11isAvailableEv(&list), "an initialized list is available");
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0x00850003, "a second initialize is refused");
    Require(_ZN3sce3Xml13AttributeList9terminateEv(&list) == 0 && list.impl == nullptr, "terminate");
    Require(f.Take() == "f1 f0 ", "terminate frees the table, then the object");
    Require(_ZN3sce3Xml13AttributeList9terminateEv(&list) == 0, "terminate again");

    f.Mark();
    f.FailNext(1);
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == NoMemory && list.impl == nullptr, "the object allocation fails");
    f.FailNext(2);
    Require(Throws([&] { _ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer); }), "a failed table allocation faults in the module");
    _ZN3sce3Xml13AttributeListD1Ev(&list);

    f.Mark();
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize again");
    _ZN3sce3Xml13AttributeListD2Ev(&list);
    Require(list.impl == nullptr && f.Take() == "a32 a40 f1 f0 ", "the destructor frees the table, then the object");
}

void TestAdd() {
    Fixture f;
    XmlHandle list{nullptr};
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    f.Mark();
    Require(Add(&list, "x", "1") == 0 && Add(&list, "y", "two") == 0, "addAttribute");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 2 && f.Take().empty(), "attributes take nothing from the title's allocator");
    Require(Add(&list, "x", "3") == static_cast<int>(0x8085001a), "a name that is already there");

    const XmlString x = Text("x");
    const XmlString nullBuffer{nullptr, 1};
    const XmlString empty{"x", 0};
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, nullptr, &x) == InvalidParameter, "a null name");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &x, nullptr) == InvalidParameter, "a null value");
    Require(Add(&list, nullBuffer, x) == InvalidParameter && Add(&list, x, nullBuffer) == InvalidParameter, "a null buffer");
    Require(Add(&list, empty, x) == InvalidParameter && Add(&list, x, empty) == InvalidParameter, "a size of zero");
    Require(Add(&list, XmlString{"\0z", 2}, x) == InvalidParameter, "a name that starts with a terminator is empty");

    Require(Add(&list, XmlString{"abc", 1}, XmlString{"four\0five", 1}) == 0, "the size of a short name and of a value is ignored");
    Require(ByName(&list, Text("abc")) == "abc=four", "both are read up to their terminator");
    Require(Add(&list, XmlString{"ef\0gh", 5}, XmlString{"\0", 1}) == 0, "a value that is only a terminator");
    Require(ByName(&list, Text("ef")) == "ef=", "the name stops at the terminator and the value is empty");

    const std::string longName(200, 'A');
    Require(Add(&list, XmlString{longName.c_str(), longName.size()}, x) == 0, "a name over 128 bytes");
    Require(ByName(&list, XmlString{longName.c_str(), 128}) == std::string(128, 'A') + "=x", "the name is cut to 128 bytes");
    Require(ByName(&list, XmlString{longName.c_str(), longName.size()}) == "-", "the whole name finds nothing");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 5, "five attributes");

    _ZN3sce3Xml13AttributeList5clearEv(&list);
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 0 && _ZNK3sce3Xml13AttributeList11isAvailableEv(&list), "clear empties the list");
    Require(Add(&list, "x", "again") == 0 && ByIndex(&list, 0) == "x=again", "a cleared list takes attributes again");
    _ZN3sce3Xml13AttributeListD1Ev(&list);
}

void TestLookup() {
    Fixture f;
    XmlHandle list{nullptr};
    Require(ByName(&list, Text("x")) == "-" && ByIndex(&list, 0) == "-", "an uninitialized list finds nothing");
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    Require(ByIndex(&list, 0) == "-", "index 0 of an empty list finds nothing");
    Require(Throws([&] { ByIndex(&list, 1); }), "an index past the end faults in the module");
    Require(Add(&list, "x", "1") == 0 && Add(&list, "yz", "two") == 0, "addAttribute");

    f.Mark();
    XmlHandle attr{reinterpret_cast<void*>(0x5555)};
    _ZNK3sce3Xml13AttributeList12getAttributeEj(&attr, &list, 1);
    Require(f.Take() == "a32 " && _ZNK3sce3Xml4Attr11isAvailableEv(&attr), "the result is an Attr from the list's initializer");
    Require(Name(&attr) == "yz" && Value(&attr) == "two", "the Attr holds the name and the value");
    _ZN3sce3Xml4AttrD1Ev(&attr);
    Require(f.Take() == "f0 ", "the Attr frees its block");

    Require(ByIndex(&list, 0) == "x=1" && ByIndex(&list, 2) == "-", "by index");
    Require(Throws([&] { ByIndex(&list, 3); }), "an index past the end");
    Require(ByName(&list, Text("yz")) == "yz=two" && ByName(&list, Text("w")) == "-", "by name");
    Require(ByName(&list, XmlString{"xyz", 1}) == "x=1", "the name is compared over its size");
    Require(ByName(&list, XmlString{"x", 0}) == "-" && ByName(&list, XmlString{nullptr, 1}) == "-", "an empty name finds nothing");
    XmlHandle none{reinterpret_cast<void*>(0x5555)};
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&none, &list, nullptr);
    Require(none.impl == nullptr, "a null name finds nothing");

    f.Mark();
    f.FailNext(1);
    Require(ByIndex(&list, 0) == "-", "a failed Attr allocation gives an empty Attr");
    _ZN3sce3Xml13AttributeListD1Ev(&list);
}

void TestCopy() {
    Fixture f;
    XmlHandle list{nullptr};
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    Require(Add(&list, "x", "1") == 0, "addAttribute");
    f.Mark();
    XmlHandle copy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml13AttributeListC1ERKS1_(&copy, &list);
    Require(f.Take() == "a32 a40 " && copy.impl != list.impl, "a copy is a separate object with its own table");
    Require(ByIndex(&copy, 0) == "x=1", "a copy has the attributes");
    Require(Add(&copy, "y", "2") == 0 && _ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 1, "the copy is independent");
    _ZN3sce3Xml13AttributeListD1Ev(&copy);

    f.Mark();
    f.FailNext(1);
    _ZN3sce3Xml13AttributeListC2ERKS1_(&copy, &list);
    Require(copy.impl == nullptr, "a failed object allocation leaves the copy empty");
    f.FailNext(2);
    _ZN3sce3Xml13AttributeListC2ERKS1_(&copy, &list);
    Require(copy.impl != nullptr && !_ZNK3sce3Xml13AttributeList11isAvailableEv(&copy), "a failed table allocation leaves a copy without a table");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&copy) == 0 && Add(&copy, "x", "1") == NotInitialized && ByIndex(&copy, 0) == "-", "a list without a table");
    _ZN3sce3Xml13AttributeListD1Ev(&copy);
    Require(copy.impl == nullptr && f.Take() == "a32 a32 a40 f0 ", "the destructor frees the object");

    XmlHandle blank{nullptr};
    Require(Throws([&] { _ZN3sce3Xml13AttributeListC1ERKS1_(&copy, &blank); }), "copying an uninitialized list faults in the module");
    _ZN3sce3Xml13AttributeListD1Ev(&list);
}

void TestAttr() {
    Fixture f;
    f.Mark();
    XmlHandle attr{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml4AttrC1Ev(&attr);
    Require(attr.impl == nullptr && !_ZNK3sce3Xml4Attr11isAvailableEv(&attr), "the constructor clears the object");
    Require(Name(&attr) == "<null>" && Value(&attr) == "<null>", "an uninitialized Attr has a null name and value");
    Require(_ZN3sce3Xml4Attr9terminateEv(&attr) == NotInitialized, "terminate before initialize");
    const XmlString x = Text("x");
    Require(_ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&attr, &x) == NotInitialized, "setName before initialize");
    Require(_ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&attr, nullptr) == InvalidParameter, "the string is checked before the object");
    XmlHandle never{nullptr};
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, nullptr) == NotInitialized, "a null initializer");
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &never) == NotInitialized, "an initializer without an implementation");
    f.FailNext(1);
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == NoMemory && attr.impl == nullptr, "a failed allocation");
    f.Mark();

    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == 0 && f.Take() == "a32 ", "initialize allocates the object");
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == 0x00850003, "a second initialize is refused");
    Require(_ZNK3sce3Xml4Attr11isAvailableEv(&attr) && Name(&attr).empty() && Value(&attr).empty(), "a new Attr has an empty name and value");

    const XmlString nullBuffer{nullptr, 1};
    const XmlString empty{"x", 0};
    Require(_ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&attr, &nullBuffer) == InvalidParameter && _ZN3sce3Xml4Attr8setValueEPKNS0_6StringE(&attr, &empty) == InvalidParameter, "a null buffer or a size of zero");
    const XmlString name{"a\0b", 3};
    const XmlString value = Text("green");
    Require(_ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&attr, &name) == 0 && _ZN3sce3Xml4Attr8setValueEPKNS0_6StringE(&attr, &value) == 0, "setName and setValue");
    Require(Name(&attr) == std::string("a\0b", 3) && Value(&attr) == "green", "the whole size is kept, terminators included");

    f.Mark();
    XmlHandle copy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml4AttrC1ERKS1_(&copy, &attr);
    Require(f.Take() == "a32 " && Name(&copy) == std::string("a\0b", 3) && Value(&copy) == "green", "a copy has its own object");
    Require(_ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&copy, &x) == 0 && Name(&attr) == std::string("a\0b", 3), "the copy is independent");
    XmlHandle blank{nullptr};
    XmlHandle blankCopy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml4AttrC2ERKS1_(&blankCopy, &blank);
    Require(blankCopy.impl == nullptr, "a copy of an empty Attr is empty");

    XmlHandle target{nullptr};
    Require(_ZN3sce3Xml4AttraSERKS1_(&target, &attr) == &target && f.Take() == "a32 ", "operator= into an empty Attr allocates");
    Require(_ZN3sce3Xml4AttraSERKS1_(&target, &copy) == &target && f.Take().empty() && Name(&target) == "x", "operator= into a live Attr copies");
    Require(_ZN3sce3Xml4AttraSERKS1_(&target, &target) == &target, "self assignment");
    Require(_ZN3sce3Xml4AttraSERKS1_(&target, &blank) == &target && target.impl == nullptr && f.Take() == "f1 ", "operator= from an empty Attr frees the object");

    Require(_ZN3sce3Xml4Attr9terminateEv(&copy) == 0 && copy.impl == nullptr && f.Take() == "f0 ", "terminate frees the object");
    Require(_ZN3sce3Xml4Attr9terminateEv(&copy) == NotInitialized, "terminate again");
    _ZN3sce3Xml4AttrD2Ev(&attr);
    Require(attr.impl == nullptr, "the destructor clears the object");
}

void TestInitializerGone() {
    Fixture f;
    XmlHandle list{nullptr};
    XmlHandle attr{nullptr};
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize a list");
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == 0, "initialize an Attr");
    Require(Add(&list, "x", "1") == 0, "addAttribute");
    Require(_ZN3sce3Xml11Initializer9terminateEv(&f.initializer) == 0, "terminate the initializer");
    Require(_ZN3sce3Xml13AttributeList9terminateEv(&list) == 0 && list.impl != nullptr, "terminate leaves the list alone once the initializer is gone");
    Require(ByIndex(&list, 0) == "-", "getAttribute cannot initialize its Attr");
    Require(Throws([&] { _ZN3sce3Xml13AttributeListD1Ev(&list); }), "the destructor faults in the module freeing the table");
    Require(Throws([&] { _ZN3sce3Xml4Attr9terminateEv(&attr); }), "terminating an Attr faults in the module");
}

class GuardedPage {
public:
    char* data = nullptr;
    std::size_t size = 0;
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "allocate two pages");
        DWORD previous = 0;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0, "make the second page inaccessible");
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "the page size");
        size = static_cast<std::size_t>(pageSize);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "allocate two pages");
        data = static_cast<char*>(mapping);
        Require(mprotect(data + size, size, PROT_NONE) == 0, "make the second page inaccessible");
#endif
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
    char* End(const char* bytes, std::size_t count) {
        char* start = data + size - count;
        std::memcpy(start, bytes, count);
        return start;
    }
};

void TestUnreadableString() {
    Fixture f;
    XmlHandle list{nullptr};
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    GuardedPage page;
    const XmlString value = Text("one");
    const XmlString terminated{page.End("alpha", 6), 5};
    Require(Add(&list, terminated, value) == 0, "a name whose terminator is the last readable byte");
    const XmlString last{page.End("alpha", 5), 5};
    Require(ByName(&list, last) == "alpha=one", "a lookup reads only the size it is given");
    Require(Throws([&] { Add(&list, last, value); }), "a name with no terminator before the unreadable page faults in the module");
    const XmlString past{page.data + page.size - 2, 5};
    Require(Throws([&] { ByName(&list, past); }), "a lookup that runs past the readable page");
    XmlHandle attr{nullptr};
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == 0, "initialize an Attr");
    Require(Throws([&] { _ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&attr, &past); }), "setName past the readable page");
    _ZN3sce3Xml4AttrD1Ev(&attr);
    _ZN3sce3Xml13AttributeListD1Ev(&list);
}

}

int main() {
    TestListLifecycle();
    TestAdd();
    TestLookup();
    TestCopy();
    TestAttr();
    TestInitializerGone();
    TestUnreadableString();
    std::puts("guest_xml_attribute_list: all checks passed");
    return 0;
}
