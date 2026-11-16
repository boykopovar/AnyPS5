#include "prx/libSceXml/include/Api.hpp"
#include "prx/libSceXml/include/InventoryExports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
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
};

void* APS5_VABI TestAllocate(XmlMemAllocator* self, std::size_t size, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "allocate receives the user data");
    Require(size != 0, "allocate receives a non-zero size");
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

    std::size_t LiveBlocks() const {
        std::size_t live = 0;
        for (void* block : allocator.blocks) {
            if (block != nullptr) ++live;
        }
        return live;
    }
};

XmlString Text(const char* text) {
    return XmlString{text, text != nullptr ? std::strlen(text) : 0};
}

std::string Read(const XmlString& value) {
    return value.buffer != nullptr ? std::string(value.buffer, value.size) : std::string("<null>");
}

void TestAttributeListLifecycle() {
    Fixture f;
    XmlHandle list{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml13AttributeListC1Ev(&list);
    Require(list.impl == nullptr, "the constructor clears the object");
    Require(!_ZNK3sce3Xml13AttributeList11isAvailableEv(&list), "an uninitialized list is not available");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 0, "an uninitialized list has no attributes");
    Require(_ZN3sce3Xml13AttributeList9terminateEv(&list) == static_cast<int>(0x80850002), "terminate before initialize");

    XmlHandle never{nullptr};
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, nullptr) == static_cast<int>(0x80850002), "a null initializer is not initialized");
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &never) == static_cast<int>(0x80850002), "an empty initializer is not initialized");
    Require(list.impl == nullptr, "a refused initialize leaves the object empty");

    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    Require(list.impl != nullptr, "initialize sets the object");
    Require(_ZNK3sce3Xml13AttributeList11isAvailableEv(&list), "an initialized list is available");
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0x00850003, "a second initialize is refused");

    Require(_ZN3sce3Xml13AttributeList9terminateEv(&list) == 0, "terminate");
    Require(list.impl == nullptr, "terminate clears the object");
    Require(!_ZNK3sce3Xml13AttributeList11isAvailableEv(&list), "a terminated list is not available");
    Require(_ZN3sce3Xml13AttributeList9terminateEv(&list) == static_cast<int>(0x80850002), "terminate again");
}

void TestAddAndRead() {
    Fixture f;
    XmlHandle list{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&list);
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 0, "a new list is empty");

    const XmlString name = Text("alpha");
    const XmlString value = Text("one");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &name, &value) == 0, "addAttribute");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 1, "one attribute");

    const XmlString second = Text("beta");
    const XmlString secondValue = Text("two");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &second, &secondValue) == 0, "addAttribute");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 2, "two attributes");

    XmlString result{};
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result, &list, &name);
    Require(Read(result) == "one", "getAttribute by name returns the value");
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result, &list, &second);
    Require(Read(result) == "two", "getAttribute by name returns the second value");

    const XmlString missing = Text("gamma");
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result, &list, &missing);
    Require(result.buffer == nullptr && result.size == 0, "an unknown name finds nothing");

    _ZNK3sce3Xml13AttributeList12getAttributeEj(&result, &list, 0);
    Require(Read(result) == "one", "getAttribute by index 0");
    _ZNK3sce3Xml13AttributeList12getAttributeEj(&result, &list, 1);
    Require(Read(result) == "two", "getAttribute by index 1");
    _ZNK3sce3Xml13AttributeList12getAttributeEj(&result, &list, 2);
    Require(result.buffer == nullptr && result.size == 0, "an index past the end finds nothing");

    const XmlString emptyName = Text("");
    const XmlString emptyValue = Text("");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &emptyName, &emptyValue) == 0, "an empty name and value");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 3, "three attributes");

    Require(_ZN3sce3Xml13AttributeList5clearEv(&list) == 0, "clear");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 0, "clear empties the list");
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result, &list, &name);
    Require(result.buffer == nullptr && result.size == 0, "a cleared list finds nothing");
}

void TestCopyAndAssign() {
    Fixture f;
    XmlHandle list{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&list);
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    const XmlString name = Text("alpha");
    const XmlString value = Text("one");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &name, &value) == 0, "addAttribute");

    XmlHandle copy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml13AttributeListC1ERKS1_(&copy, &list);
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&copy) == 1, "a copy has the attributes");
    XmlString result{};
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result, &copy, &name);
    Require(Read(result) == "one", "a copy reads the same value");
    Require(copy.impl != list.impl, "a copy is a separate object");

    const XmlString second = Text("beta");
    const XmlString secondValue = Text("two");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&copy, &second, &secondValue) == 0, "addAttribute to the copy");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&copy) == 2, "the copy grew");
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&list) == 1, "the original did not grow");

    XmlHandle empty{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml13AttributeListC1ERKS1_(&empty, &copy);
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&empty) == 2, "copying a copy");

    XmlHandle blank{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&blank);
    XmlHandle blankCopy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml13AttributeListC1ERKS1_(&blankCopy, &blank);
    Require(blankCopy.impl == nullptr, "a copy of an empty list is empty");

    XmlHandle target{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&target);
    XmlHandle assigned{nullptr};
    _ZN3sce3Xml13AttributeListC1ERKS1_(&assigned, &list);
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&assigned) == 1, "a copy of a live list");
    XmlString result2{};
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result2, &assigned, &name);
    Require(Read(result2) == "one", "the copy of a live list reads the value");

    XmlHandle other{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&other);
    XmlHandle otherCopy{nullptr};
    _ZN3sce3Xml13AttributeListC1ERKS1_(&otherCopy, &copy);
    Require(_ZNK3sce3Xml13AttributeList9getLengthEv(&otherCopy) == 2, "a copy of a copy");

    _ZN3sce3Xml13AttributeListD1Ev(&otherCopy);
    _ZN3sce3Xml13AttributeListD1Ev(&other);
    _ZN3sce3Xml13AttributeListD1Ev(&assigned);
    _ZN3sce3Xml13AttributeListD1Ev(&target);
    _ZN3sce3Xml13AttributeListD1Ev(&blankCopy);
    _ZN3sce3Xml13AttributeListD1Ev(&blank);
    _ZN3sce3Xml13AttributeListD1Ev(&empty);
    _ZN3sce3Xml13AttributeListD1Ev(&copy);
    _ZN3sce3Xml13AttributeListD1Ev(&list);
}

void TestAttr() {
    Fixture f;
    XmlHandle attr{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml4AttrC1Ev(&attr);
    Require(attr.impl == nullptr, "the constructor clears the object");
    Require(!_ZNK3sce3Xml4Attr11isAvailableEv(&attr), "an uninitialized attr is not available");
    Require(_ZN3sce3Xml4Attr9terminateEv(&attr) == static_cast<int>(0x80850002), "terminate before initialize");
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, nullptr) == static_cast<int>(0x80850002), "a null initializer is not initialized");

    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == 0, "initialize");
    Require(_ZNK3sce3Xml4Attr11isAvailableEv(&attr), "an initialized attr is available");
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == 0x00850003, "a second initialize is refused");

    XmlString result{};
    _ZNK3sce3Xml4Attr7getNameEv(&result, &attr);
    Require(result.buffer == nullptr && result.size == 0, "a new attr has no name");
    _ZNK3sce3Xml4Attr8getValueEv(&result, &attr);
    Require(result.buffer == nullptr && result.size == 0, "a new attr has no value");

    const XmlString name = Text("colour");
    const XmlString value = Text("green");
    Require(_ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&attr, &name) == 0, "setName");
    Require(_ZN3sce3Xml4Attr8setValueEPKNS0_6StringE(&attr, &value) == 0, "setValue");
    _ZNK3sce3Xml4Attr7getNameEv(&result, &attr);
    Require(Read(result) == "colour", "getName returns the name");
    _ZNK3sce3Xml4Attr8getValueEv(&result, &attr);
    Require(Read(result) == "green", "getValue returns the value");

    const XmlString longer = Text("a longer value than before");
    Require(_ZN3sce3Xml4Attr8setValueEPKNS0_6StringE(&attr, &longer) == 0, "setValue again");
    _ZNK3sce3Xml4Attr8getValueEv(&result, &attr);
    Require(Read(result) == "a longer value than before", "getValue returns the new value");

    XmlHandle copy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml4AttrC1ERKS1_(&copy, &attr);
    _ZNK3sce3Xml4Attr7getNameEv(&result, &copy);
    Require(Read(result) == "colour", "a copy has the name");
    _ZNK3sce3Xml4Attr8getValueEv(&result, &copy);
    Require(Read(result) == "a longer value than before", "a copy has the value");

    const XmlString other = Text("size");
    Require(_ZN3sce3Xml4Attr7setNameEPKNS0_6StringE(&copy, &other) == 0, "setName on the copy");
    _ZNK3sce3Xml4Attr7getNameEv(&result, &attr);
    Require(Read(result) == "colour", "the original name did not change");

    XmlHandle target{nullptr};
    _ZN3sce3Xml4AttrC1Ev(&target);
    Require(_ZN3sce3Xml4AttraSERKS1_(&target, &attr) == &target, "operator= returns the object");
    _ZNK3sce3Xml4Attr7getNameEv(&result, &target);
    Require(Read(result) == "colour", "operator= copies the name");
    _ZNK3sce3Xml4Attr8getValueEv(&result, &target);
    Require(Read(result) == "a longer value than before", "operator= copies the value");
    Require(_ZN3sce3Xml4AttraSERKS1_(&target, &target) == &target, "self assignment");

    Require(_ZN3sce3Xml4Attr9terminateEv(&target) == 0, "terminate");
    Require(target.impl == nullptr, "terminate clears the object");
    Require(!_ZNK3sce3Xml4Attr11isAvailableEv(&target), "a terminated attr is not available");
    Require(_ZN3sce3Xml4Attr9terminateEv(&target) == static_cast<int>(0x80850002), "terminate again");

    _ZN3sce3Xml4AttrD1Ev(&copy);
    _ZN3sce3Xml4AttrD1Ev(&attr);
}

void TestAllocationAccounting() {
    Fixture f;
    const std::size_t before = f.LiveBlocks();
    {
        XmlHandle list{nullptr};
        _ZN3sce3Xml13AttributeListC1Ev(&list);
        Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
        Require(f.LiveBlocks() == before + 1, "initialize takes one block from the title's allocator");
        const XmlString name = Text("alpha");
        const XmlString value = Text("one");
        Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &name, &value) == 0, "addAttribute");
        Require(f.LiveBlocks() == before + 1, "adding an attribute takes no guest block");
        XmlHandle copy{nullptr};
        _ZN3sce3Xml13AttributeListC1ERKS1_(&copy, &list);
        Require(f.LiveBlocks() == before + 2, "a copy takes one block");
        _ZN3sce3Xml13AttributeListD1Ev(&copy);
        Require(f.LiveBlocks() == before + 1, "the copy's destructor frees it");
        _ZN3sce3Xml13AttributeListD2Ev(&list);
        Require(f.LiveBlocks() == before, "the destructor frees the block");
    }
    XmlHandle attr{nullptr};
    _ZN3sce3Xml4AttrC1Ev(&attr);
    Require(_ZN3sce3Xml4Attr10initializeEPKNS0_11InitializerE(&attr, &f.initializer) == 0, "initialize an attr");
    Require(f.LiveBlocks() == before + 1, "an attr takes one block");
    Require(_ZN3sce3Xml4Attr9terminateEv(&attr) == 0, "terminate the attr");
    Require(f.LiveBlocks() == before, "terminate frees it");
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
    _ZN3sce3Xml13AttributeListC1Ev(&list);
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "initialize");
    GuardedPage page;
    const XmlString readable = XmlString{page.End("alpha", 5), 5};
    const XmlString value = Text("one");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &readable, &value) == 0, "addAttribute from the last readable bytes");
    XmlString result{};
    _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result, &list, &readable);
    Require(Read(result) == "one", "getAttribute from the last readable bytes");

    const XmlString past = XmlString{page.data + page.size - 2, 8};
    bool threw = false;
    try {
        _ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &past, &value);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw, "a name that runs past the readable page throws instead of faulting the host");
    threw = false;
    try {
        _ZNK3sce3Xml13AttributeList12getAttributeEPKNS0_6StringE(&result, &list, &past);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw, "getAttribute on a name past the readable page throws");

    const XmlString nullBuffer = XmlString{nullptr, 4};
    threw = false;
    try {
        _ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&list, &nullBuffer, &value);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "a null buffer with a non-zero size throws");
}

}

int main() {
    TestAttributeListLifecycle();
    TestAddAndRead();
    TestCopyAndAssign();
    TestAttr();
    TestAllocationAccounting();
    TestUnreadableString();
    std::puts("guest_xml_attribute_list: all checks passed");
    return 0;
}
