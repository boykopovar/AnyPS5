#include "prx/libSceXml/include/Api.hpp"
#include "prx/libSceXml/include/InventoryExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
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
    std::size_t first = 0;
    std::string log;
    std::size_t calls = 0;
    std::size_t failAt = SIZE_MAX;
};

void* APS5_VABI TestAllocate(XmlMemAllocator* self, std::size_t size, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "allocate receives the user data");
    if (allocator->calls++ == allocator->failAt) return nullptr;
    allocator->log += "a" + std::to_string(size) + " ";
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
    allocator->log += "f" + std::to_string(static_cast<long long>(index) - static_cast<long long>(allocator->first)) + " ";
    std::free(pointer);
    allocator->blocks[index] = nullptr;
}

void APS5_VABI TestDestroy(XmlMemAllocator*) {}

const XmlMemAllocatorVtable g_testVtable{TestDestroy, TestDestroy, TestAllocate, TestDeallocate};

struct Fixture {
    TestAllocator allocator{};
    XmlInitParameter parameter{};
    XmlHandle initializer{nullptr};
    XmlHandle builder{nullptr};

    Fixture() {
        allocator.base.vtable = &g_testVtable;
        parameter = {&allocator.base, reinterpret_cast<void*>(0x1234), {0, 0}};
        Require(_ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0, "the initializer starts");
        Require(_ZN3sce3Xml3Dom15DocumentBuilder10initializeEPKNS0_11InitializerE(&builder, &initializer) == 0, "the builder starts");
    }

    ~Fixture() {
        _ZN3sce3Xml3Dom15DocumentBuilder9terminateEv(&builder);
        _ZN3sce3Xml11Initializer9terminateEv(&initializer);
        for (void* block : allocator.blocks) {
            if (block != nullptr) std::free(block);
        }
    }

    void Mark() {
        allocator.log.clear();
        allocator.first = allocator.blocks.size();
    }

    int Parse(const char* text) {
        const XmlString string{text, std::strlen(text)};
        return _ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(&builder, &string, true);
    }
};

XmlNodeId Root(const XmlHandle* document) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document7getRootEv(&id, document);
    return id;
}

XmlNodeId FirstChild(const XmlHandle* document, XmlNodeId node) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE(&id, document, node);
    return id;
}

XmlNodeId Parent(const XmlHandle* document, XmlNodeId node) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document9getParentENS1_6NodeIdE(&id, document, node);
    return id;
}

std::string Name(const XmlHandle* document, XmlNodeId node) {
    XmlString name{};
    _ZNK3sce3Xml3Dom8Document11getNodeNameENS1_6NodeIdE(&name, document, node);
    return name.buffer != nullptr ? std::string(name.buffer, name.size) : std::string("<null>");
}

std::string Attribute(const XmlHandle* document, XmlNodeId node, const char* text, std::size_t size) {
    XmlString value{};
    const XmlString name{text, size};
    _ZNK3sce3Xml3Dom8Document12getAttributeENS1_6NodeIdEPKNS0_6StringE(&value, document, node, text != nullptr ? &name : nullptr);
    return value.buffer != nullptr ? std::string(value.buffer, value.size) : std::string("<null>");
}

std::uintptr_t Offset(XmlNodeId node) {
    return reinterpret_cast<std::uintptr_t>(node.second);
}

void TestLifecycle() {
    Fixture f;
    XmlHandle d1{nullptr};
    XmlHandle d2{nullptr};
    XmlHandle d3{nullptr};
    f.Mark();
    Require(_ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&d1, &f.builder) == &d1 && d1.impl == nullptr && f.allocator.log.empty(), "no document before a parse");
    Require(f.Parse("<r a=\"1\"><b/></r>") == 0, "parse");
    f.Mark();
    const std::size_t builderBlock = f.allocator.blocks.size();
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&d1, &f.builder);
    Require(d1.impl != nullptr, "a document after a parse");
    Require(f.allocator.log == "a72 a72 a1240 a96 a2088 a5304 a2088 a1752 a1752 f8 f7 f6 f5 f4 f3 f2 ", "the first document allocates the builder block, its own block and seven scratch blocks");
    f.Mark();
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&d2, &f.builder);
    Require(f.allocator.log == "a72 a1240 a96 a2088 a5304 a2088 a1752 a1752 f7 f6 f5 f4 f3 f2 f1 ", "a later document allocates its own block and seven scratch blocks");
    f.Mark();
    _ZN3sce3Xml3Dom8DocumentC1ERKS2_(&d3, &d1);
    Require(f.allocator.log == "a72 a1240 a96 a2088 a5304 a2088 a1752 a1752 f7 f6 f5 f4 f3 f2 f1 ", "a copy allocates like a document");
    Require(Offset(Root(&d3)) == 2 && Root(&d3).first == Root(&d1).first, "a copy reads the same document");
    f.Mark();
    Require(_ZN3sce3Xml3Dom8DocumentaSERKS2_(&d3, &d2) == &d3 && f.allocator.log.empty(), "assigning between documents allocates nothing");
    Require(_ZN3sce3Xml3Dom8DocumentaSERKS2_(&d3, &d3) == &d3, "self assignment");
    Require(_ZNK3sce3Xml3Dom8Document11isAvailableEv(&d1) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&d1) == 0 && _ZNK3sce3Xml3Dom8Document10isReadOnlyEv(&d1), "a parsed document");
    XmlNodeId docRoot{};
    _ZNK3sce3Xml3Dom8Document10getDocRootEv(&docRoot, &d1);
    Require(Offset(docRoot) == 1 && Offset(Root(&d1)) == 2 && Name(&d1, Root(&d1)) == "r", "the document and root nodes");
    Require(Offset(FirstChild(&d1, Root(&d1))) == 13 && Name(&d1, FirstChild(&d1, Root(&d1))) == "b", "the first child");
    f.Mark();
    _ZN3sce3Xml3Dom8DocumentD1Ev(&d3);
    Require(d3.impl == nullptr && f.allocator.log == "f-8 ", "the destructor frees the document block");
    f.Mark();
    Require(_ZN3sce3Xml3Dom8Document9terminateEv(&d2) == 0 && d2.impl == nullptr, "terminate");
    Require(_ZN3sce3Xml3Dom8Document9terminateEv(&d2) == static_cast<int>(0x80850002), "terminate again");
    Require(f.Parse("<x/>") == 0, "a new document");
    Require(_ZNK3sce3Xml3Dom8Document11isAvailableEv(&d1), "an older document is still available");
    _ZN3sce3Xml3Dom8DocumentD1Ev(&d1);
    f.Mark();
    Require(_ZN3sce3Xml3Dom15DocumentBuilder9terminateEv(&f.builder) == 0, "terminate the builder");
    const std::string first = "f" + std::to_string(static_cast<long long>(builderBlock) - static_cast<long long>(f.allocator.first)) + " ";
    Require(f.allocator.log.compare(0, first.size(), first) == 0, "the builder frees its document block first");
}

void TestEmptyDocument() {
    XmlHandle empty{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Dom8DocumentC1Ev(&empty);
    const XmlNodeId node{reinterpret_cast<void*>(0x1234), reinterpret_cast<void*>(2), 0};
    XmlNodeId id{reinterpret_cast<void*>(1), reinterpret_cast<void*>(1), 1};
    _ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE(&id, &empty, node);
    Require(id.first == nullptr && id.second == nullptr && id.flag == 0, "an empty document has no nodes");
    const XmlNodeId& invalid = _ZN3sce3Xml3Dom13invalidNodeIdE;
    Require(invalid.first == nullptr && invalid.second == nullptr && invalid.flag == 0, "the invalid node id is zero");
    Require(Name(&empty, node) == "<null>" && Attribute(&empty, node, "a", 1) == "<null>", "an empty document has no strings");
    Require(_ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&empty, node) == -1 && _ZNK3sce3Xml3Dom8Document13getEntityTypeENS1_6NodeIdE(&empty, node) == 0, "an empty document's node types");
    Require(!_ZNK3sce3Xml3Dom8Document13hasChildNodesENS1_6NodeIdE(&empty, node) && !_ZNK3sce3Xml3Dom8Document13hasAttributesENS1_6NodeIdE(&empty, node), "an empty document's flags");
    Require(!_ZNK3sce3Xml3Dom8Document11isAvailableEv(&empty) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&empty) == static_cast<int>(0x80850002) && _ZNK3sce3Xml3Dom8Document10isReadOnlyEv(&empty), "an empty document's state");
    Require(_ZN3sce3Xml3Dom8Document9terminateEv(&empty) == static_cast<int>(0x80850002), "terminating an empty document");
    XmlHandle copy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Dom8DocumentC1ERKS2_(&copy, &empty);
    Require(copy.impl == nullptr, "a copy of an empty document is empty");
    Fixture f;
    Require(f.Parse("<r/>") == 0, "parse");
    XmlHandle live{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&live, &f.builder);
    f.Mark();
    _ZN3sce3Xml3Dom8DocumentaSERKS2_(&copy, &live);
    Require(copy.impl != nullptr && f.allocator.log == "a72 a1240 a96 a2088 a5304 a2088 a1752 a1752 f7 f6 f5 f4 f3 f2 f1 ", "assigning into an empty document allocates like a copy");
    _ZN3sce3Xml3Dom8DocumentD1Ev(&copy);
    _ZN3sce3Xml3Dom8DocumentD1Ev(&live);
    XmlHandle unbuilt{nullptr};
    XmlHandle noBuilder{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&unbuilt, &noBuilder);
    Require(unbuilt.impl == nullptr, "a builder that is not initialized has no document");
}

void TestStatusAndAttributes() {
    Fixture f;
    Require(f.Parse("<r a=\"1\" ab=\"2\" b=\"&amp;\"><c/>t</r>") == 0, "parse");
    XmlHandle document{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
    const XmlNodeId root = Root(&document);
    Require(Attribute(&document, root, "a", 1) == "1" && Attribute(&document, root, "ab", 2) == "2" && Attribute(&document, root, "b", 1) == "&amp;", "attribute values");
    Require(Attribute(&document, root, "A", 1) == "<null>" && Attribute(&document, root, "", 0) == "<null>" && Attribute(&document, root, nullptr, 0) == "<null>", "attributes that are not there");
    Require(Attribute(&document, root, "a\0x", 3) == "1", "the name is compared up to its first zero byte");
    Require(Attribute(&document, root, "a", 0) == "<null>", "a name with size 0 finds nothing");
    XmlString value{};
    const XmlString noBuffer{nullptr, 1};
    _ZNK3sce3Xml3Dom8Document12getAttributeENS1_6NodeIdEPKNS0_6StringE(&value, &document, root, &noBuffer);
    Require(value.buffer == nullptr && value.size == 0, "a name without a buffer finds nothing");
    XmlNodeId docRoot{};
    _ZNK3sce3Xml3Dom8Document10getDocRootEv(&docRoot, &document);
    Require(Attribute(&document, docRoot, "a", 1) == "<null>", "only elements have attributes");
    Parent(&document, XmlNodeId{});
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&document) == static_cast<int>(0x80850003), "getParent of no node sets an invalid-node status");
    Require(Offset(Parent(&document, FirstChild(&document, root))) == Offset(root), "getParent finds the parent");
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&document) == 0, "getParent clears the status");
    XmlNodeId last{};
    _ZNK3sce3Xml3Dom8Document12getLastChildENS1_6NodeIdE(&last, &document, FirstChild(&document, root));
    Require(last.first == root.first && last.second == nullptr && last.flag == 0, "getLastChild of a node without children keeps the document in the id");
    Parent(&document, XmlNodeId{root.first, reinterpret_cast<void*>(4), 0});
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&document) == static_cast<int>(0x80850201), "getParent of a node it cannot reach sets a not-found status");
    _ZN3sce3Xml3Dom8Document11resetStatusEv(&document);
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&document) == 0, "resetStatus clears a not-found status");
    Parent(&document, XmlNodeId{});
    _ZN3sce3Xml3Dom8Document11resetStatusEv(&document);
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&document) == 0, "resetStatus clears an invalid-node status");
    _ZN3sce3Xml3Dom8Document11resetStatusEv(&document);
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&document) == 0, "resetStatus on a clear status keeps it clear");
    XmlHandle copy{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1ERKS2_(&copy, &document);
    Parent(&document, XmlNodeId{});
    _ZN3sce3Xml3Dom8Document11resetStatusEv(&copy);
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&document) == 0, "a copy shares the status it resets");
    _ZN3sce3Xml3Dom8DocumentD1Ev(&copy);
    XmlHandle empty{nullptr};
    _ZN3sce3Xml3Dom8Document11resetStatusEv(&empty);
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&empty) == static_cast<int>(0x80850002), "resetStatus leaves an empty document as it was");
    _ZN3sce3Xml3Dom8DocumentD1Ev(&document);
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

void TestAttributeNameAtPageEnd() {
    Fixture f;
    Require(f.Parse("<r a=\"1\" ab=\"2\" b=\"&amp;\"/>") == 0, "parse");
    XmlHandle document{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
    const XmlNodeId root = Root(&document);
    GuardedPage page;
    Require(Attribute(&document, root, page.End("ab", 3), 3) == "2", "a name whose zero byte is the last readable byte");
    Require(Attribute(&document, root, page.End("z", 1), 1) == "<null>", "a name compared only up to its first differing byte, which is the last readable byte");
    bool faulted = false;
    try {
        Attribute(&document, root, page.End("ab", 2), 2);
    } catch (const std::logic_error&) {
        faulted = true;
    }
    Require(faulted, "a name that matches up to the end of the readable page is read past it, where 13.42 faults");
    _ZN3sce3Xml3Dom8DocumentD1Ev(&document);
}

void TestAllocationFailures() {
    for (std::size_t k = 0; k < 9; ++k) {
        Fixture f;
        Require(f.Parse("<r/>") == 0, "parse");
        f.Mark();
        f.allocator.calls = 0;
        f.allocator.failAt = k;
        XmlHandle document{nullptr};
        try {
            _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
        } catch (const std::exception&) {
        }
        f.allocator.failAt = SIZE_MAX;
        Require(document.impl == nullptr, "a getDocument whose allocation fails leaves no document");
        for (std::size_t i = f.allocator.first + (k > 0 ? 1 : 0); i < f.allocator.blocks.size(); ++i) Require(f.allocator.blocks[i] == nullptr, "a getDocument whose allocation fails frees the document's blocks");
        Require(_ZN3sce3Xml3Dom15DocumentBuilder9terminateEv(&f.builder) == 0, "terminate the builder");
        for (std::size_t i = f.allocator.first; i < f.allocator.blocks.size(); ++i) Require(f.allocator.blocks[i] == nullptr, "terminate frees the builder block");
    }
}

bool SameId(XmlNodeId a, XmlNodeId b) {
    return a.first == b.first && a.second == b.second && a.flag == b.flag;
}

bool IsNullId(XmlNodeId id) {
    return id.first == nullptr && id.second == nullptr && id.flag == 0;
}

XmlString Str(const char* text) {
    return XmlString{text, text != nullptr ? std::strlen(text) : 0};
}

std::string TextOf(const XmlHandle* document, XmlNodeId node) {
    XmlString text{};
    _ZNK3sce3Xml3Dom8Document7getTextENS1_6NodeIdE(&text, document, node);
    return text.buffer != nullptr ? std::string(text.buffer, text.size) : std::string("<null>");
}

std::string SkippedTextOf(const XmlHandle* document, XmlNodeId node) {
    XmlString text{};
    _ZNK3sce3Xml3Dom8Document14getSkippedTextENS1_6NodeIdE(&text, document, node);
    return text.buffer != nullptr ? std::string(text.buffer, text.size) : std::string("<null>");
}

std::string AttrNameOf(const XmlHandle* document, XmlNodeId attr) {
    XmlString name{};
    _ZNK3sce3Xml3Dom8Document11getAttrNameENS1_6NodeIdE(&name, document, attr);
    return name.buffer != nullptr ? std::string(name.buffer, name.size) : std::string("<null>");
}

std::string AttrValueOf(const XmlHandle* document, XmlNodeId attr) {
    XmlString value{};
    _ZNK3sce3Xml3Dom8Document12getAttrValueENS1_6NodeIdE(&value, document, attr);
    return value.buffer != nullptr ? std::string(value.buffer, value.size) : std::string("<null>");
}

XmlNodeId Sibling(const XmlHandle* document, XmlNodeId node) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document10getSiblingENS1_6NodeIdE(&id, document, node);
    return id;
}

XmlNodeId LastChild(const XmlHandle* document, XmlNodeId node) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document12getLastChildENS1_6NodeIdE(&id, document, node);
    return id;
}

XmlNodeId FirstAttr(const XmlHandle* document, XmlNodeId node) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document12getFirstAttrENS1_6NodeIdE(&id, document, node);
    return id;
}

XmlNodeId NextAttr(const XmlHandle* document, XmlNodeId attr) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document11getNextAttrENS1_6NodeIdE(&id, document, attr);
    return id;
}

void TestWritableInitializeAndLifecycle() {
    Fixture f;
    XmlHandle doc{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1Ev(&doc);
    XmlHandle uninitInit{nullptr};
    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&doc, nullptr) == static_cast<int>(0x80850002), "initialize with null initializer");
    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&doc, &uninitInit) == static_cast<int>(0x80850002), "initialize with uninitialized initializer");

    f.allocator.calls = 0;
    f.allocator.failAt = 0;
    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&doc, &f.initializer) == static_cast<int>(0x80850001), "initialize returns NoMemory on allocation failure");
    f.allocator.failAt = SIZE_MAX;
    Require(doc.impl == nullptr && !_ZNK3sce3Xml3Dom8Document11isAvailableEv(&doc), "failed initialize leaves document empty");

    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&doc, &f.initializer) == 0, "initialize succeeds");
    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&doc, &f.initializer) == 0x00850003, "second initialize reports MultipleInitialized");
    Require(_ZNK3sce3Xml3Dom8Document11isAvailableEv(&doc), "initialized document is available");
    Require(!_ZNK3sce3Xml3Dom8Document10isReadOnlyEv(&doc), "initialized document is writable");
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == 0, "initialized document status is 0");

    XmlNodeId docRoot{};
    _ZNK3sce3Xml3Dom8Document10getDocRootEv(&docRoot, &doc);
    Require(!IsNullId(docRoot) && _ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, docRoot) == 0, "docRoot has type 0");
    Require(Name(&doc, docRoot) == "<null>", "docRoot has no name");
    Require(IsNullId(Root(&doc)), "empty writable document has no root element");

    XmlHandle copy{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1ERKS2_(&copy, &doc);
    Require(!_ZNK3sce3Xml3Dom8Document10isReadOnlyEv(&copy), "copy of writable document is writable");
    const XmlString rootName = Str("root");
    XmlNodeId created{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&created, &copy, docRoot, &rootName, nullptr, nullptr);
    Require(!IsNullId(created) && SameId(Root(&doc), created), "mutation through copy is visible in original");

    _ZN3sce3Xml3Dom8DocumentD1Ev(&copy);
    Require(_ZN3sce3Xml3Dom8Document9terminateEv(&doc) == 0, "terminate writable document");
    Require(_ZN3sce3Xml3Dom8Document9terminateEv(&doc) == static_cast<int>(0x80850002), "terminate again");
}

void TestWritableBuildAndEdit() {
    Fixture f;
    XmlHandle doc{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1Ev(&doc);
    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&doc, &f.initializer) == 0, "initialize");
    XmlNodeId docRoot{};
    _ZNK3sce3Xml3Dom8Document10getDocRootEv(&docRoot, &doc);

    XmlHandle attrs{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&attrs);
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&attrs, &f.initializer) == 0, "attrs init");
    const XmlString k = Str("k");
    const XmlString v = Str("v");
    const XmlString m = Str("m");
    const XmlString n = Str("n");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&attrs, &k, &v) == 0, "add k=v");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&attrs, &m, &n) == 0, "add m=n");

    const XmlString rootStr = Str("root");
    XmlNodeId root{};
    _ZN3sce3Xml3Dom8Document13createElementEPKNS0_6StringEPKNS0_13AttributeListES5_(&root, &doc, &rootStr, &attrs, nullptr);
    Require(!IsNullId(root), "createElement succeeds");
    Require(_ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, root) == 5, "created element has type 5");
    Require(Name(&doc, root) == "root", "created element name");
    Require(Attribute(&doc, root, "k", 1) == "v" && Attribute(&doc, root, "m", 1) == "n", "created element attributes");
    Require(IsNullId(Root(&doc)), "createElement does not attach to tree");

    XmlNodeId inserted{};
    _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_(&inserted, &doc, docRoot, XmlNodeId{}, root);
    Require(SameId(inserted, root), "insertNode returns inserted node");
    Require(SameId(Root(&doc), root) && SameId(FirstChild(&doc, docRoot), root), "root attached under docRoot");
    Require(IsNullId(Parent(&doc, root)) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == 0, "root parent is null with status 0");

    const XmlString childStr = Str("c");
    const XmlString txtStr = Str("txt");
    XmlNodeId child{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&child, &doc, root, &childStr, &attrs, &txtStr);
    Require(!IsNullId(child) && _ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, child) == 5, "addElementChild creates type 5 element");
    Require(SameId(Parent(&doc, child), root), "child parent is root");
    const XmlNodeId childText = FirstChild(&doc, child);
    Require(!IsNullId(childText) && _ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, childText) == 8, "addElementChild text child");
    Require(TextOf(&doc, childText) == "txt" && Name(&doc, childText) == "txt", "text child value");

    const XmlString helloStr = Str("hello");
    const XmlString firstStr = Str("first");
    const XmlString zeroStr = Str("zero");
    XmlNodeId t1{};
    XmlNodeId t2{};
    XmlNodeId t3{};
    _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE(&t1, &doc, &helloStr);
    _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE(&t2, &doc, &firstStr);
    _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE(&t3, &doc, &zeroStr);
    Require(!IsNullId(t1) && _ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, t1) == 8, "createTextNode type 8");

    XmlNodeId ins1{};
    XmlNodeId ins2{};
    XmlNodeId ins3{};
    _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_(&ins1, &doc, root, child, t1);
    _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_(&ins2, &doc, root, child, t2);
    _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_(&ins3, &doc, root, XmlNodeId{}, t3);
    Require(SameId(ins1, t1) && SameId(ins2, t2) && SameId(ins3, t3), "insertNode returns each inserted child");
    Require(SameId(FirstChild(&doc, root), t3), "null refChild inserts first");
    Require(SameId(Sibling(&doc, t3), child), "second child is c");
    Require(SameId(Sibling(&doc, child), t2), "insertNode after c puts t2 right after c");
    Require(SameId(Sibling(&doc, t2), t1), "t1 follows t2");
    Require(IsNullId(Sibling(&doc, t1)) && SameId(LastChild(&doc, root), t1), "t1 is last child");

    XmlNode nodeObj{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&nodeObj, root);
    Require(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&nodeObj) == 5, "Dom::Node works on writable node");
    Require(_ZNK3sce3Xml3Dom4Node13hasChildNodesEv(&nodeObj), "Dom::Node hasChildNodes on writable node");
    Require(_ZNK3sce3Xml3Dom4Node13hasAttributesEv(&nodeObj), "Dom::Node hasAttributes on writable node");
    _ZN3sce3Xml3Dom4NodeD1Ev(&nodeObj);

    XmlHandle list{nullptr};
    _ZN3sce3Xml3Dom8NodeListC1Ev(&list);
    Require(_ZN3sce3Xml3Dom8NodeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "nodeList init");
    Require(_ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE(&doc, root, &list) == 0, "getChildNodes on writable doc");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 4, "4 children under root");
    Require(SameId(_ZNK3sce3Xml3Dom8NodeList8findItemEPKNS0_6StringE(&list, &childStr), child), "NodeList::findItem by name works on writable doc");
    Require(_ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE(&doc, docRoot, &childStr, &list) == 0, "getElementsByTagName on writable doc");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 1 && SameId(_ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0), child), "getElementsByTagName finds c");
    Require(_ZNK3sce3Xml3Dom8Document13getAttributesENS1_6NodeIdEPNS1_8NodeListE(&doc, child, &list) == 0, "getAttributes on writable doc");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 2, "2 attributes on c");

    const XmlString newText = Str("new");
    Require(_ZN3sce3Xml3Dom8Document7setTextENS1_6NodeIdEPKNS0_6StringE(&doc, childText, &newText) == 0, "setText on text node");
    Require(TextOf(&doc, childText) == "new", "text updated");
    const XmlString emptyElemStr = Str("leaf");
    XmlNodeId leaf{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&leaf, &doc, root, &emptyElemStr, nullptr, nullptr);
    Require(!_ZNK3sce3Xml3Dom8Document13hasChildNodesENS1_6NodeIdE(&doc, leaf), "leaf starts without children");
    const XmlString addedText = Str("added");
    Require(_ZN3sce3Xml3Dom8Document7setTextENS1_6NodeIdEPKNS0_6StringE(&doc, leaf, &addedText) == 0, "setText on element without text creates child");
    Require(TextOf(&doc, FirstChild(&doc, leaf)) == "added", "created text child on element");
    const XmlString replacedText = Str("replaced");
    Require(_ZN3sce3Xml3Dom8Document7setTextENS1_6NodeIdEPKNS0_6StringE(&doc, leaf, &replacedText) == 0, "setText on element updates existing text child");
    Require(TextOf(&doc, FirstChild(&doc, leaf)) == "replaced" && IsNullId(Sibling(&doc, FirstChild(&doc, leaf))), "existing text child updated in place");

    const XmlString w = Str("w");
    const XmlString zz = Str("zz");
    const XmlString one = Str("1");
    const XmlString two = Str("2");
    const XmlString emptyVal{"", 0};
    Require(_ZN3sce3Xml3Dom8Document12setAttrValueENS1_6NodeIdEPKNS0_6StringES6_(&doc, child, &k, &w) == 0, "setAttrValue updates existing");
    Require(Attribute(&doc, child, "k", 1) == "w", "k updated to w");
    Require(_ZN3sce3Xml3Dom8Document12setAttrValueENS1_6NodeIdEPKNS0_6StringES6_(&doc, child, &zz, &one) == 0, "setAttrValue on missing returns 0");
    Require(Attribute(&doc, child, "zz", 2) == "<null>", "setAttrValue does not create missing attribute");
    Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&doc, child, &zz, &two) == 0, "setAttribute creates missing attribute");
    Require(Attribute(&doc, child, "zz", 2) == "2", "zz created");
    Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&doc, child, &zz, &emptyVal) == 0, "setAttribute allows empty value");
    Require(Attribute(&doc, child, "zz", 2) == "", "zz updated to empty");

    const XmlNodeId a0 = FirstAttr(&doc, child);
    const XmlNodeId a1 = NextAttr(&doc, a0);
    const XmlNodeId a2 = NextAttr(&doc, a1);
    Require(AttrNameOf(&doc, a0) == "k" && AttrValueOf(&doc, a0) == "w", "first attr");
    Require(IsNullId(Parent(&doc, a0)) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850201), "getParent on attribute reports DomNodeNotFound");
    Require(AttrNameOf(&doc, a1) == "m" && AttrValueOf(&doc, a1) == "n", "second attr");
    Require(AttrNameOf(&doc, a2) == "zz" && AttrValueOf(&doc, a2) == "" && IsNullId(NextAttr(&doc, a2)), "third attr");

    const XmlString noneStr = Str("none");
    Require(_ZN3sce3Xml3Dom8Document15removeAttributeENS1_6NodeIdEPKNS0_6StringE(&doc, child, &k) == 0, "removeAttribute k");
    Require(Attribute(&doc, child, "k", 1) == "<null>", "k removed");
    Require(_ZN3sce3Xml3Dom8Document15removeAttributeENS1_6NodeIdEPKNS0_6StringE(&doc, child, &noneStr) == 0, "removeAttribute missing is ok");

    XmlHandle replacementAttrs{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&replacementAttrs);
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&replacementAttrs, &f.initializer) == 0, "replacement attrs init");
    const XmlString pKey = Str("p");
    const XmlString pVal = Str("q");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&replacementAttrs, &pKey, &pVal) == 0, "add p=q");
    Require(_ZN3sce3Xml3Dom8Document16setAttributeListENS1_6NodeIdEPKNS0_13AttributeListE(&doc, child, &replacementAttrs) == 0, "setAttributeList replaces attributes");
    Require(Attribute(&doc, child, "m", 1) == "<null>" && Attribute(&doc, child, "p", 1) == "q", "old attrs gone, p=q present");
    Require(_ZN3sce3Xml3Dom8Document16removeAttributesENS1_6NodeIdE(&doc, child) == 0, "removeAttributes clears all");
    Require(!_ZNK3sce3Xml3Dom8Document13hasAttributesENS1_6NodeIdE(&doc, child), "no attributes remain");

    XmlNodeId removed{};
    _ZN3sce3Xml3Dom8Document11removeChildENS1_6NodeIdES3_(&removed, &doc, child, root);
    Require(SameId(removed, child), "removeChild returns detached child");
    Require(Name(&doc, child) == "c" && TextOf(&doc, childText) == "new", "detached child remains readable");
    XmlNodeId removedAgain{};
    _ZN3sce3Xml3Dom8Document11removeChildENS1_6NodeIdES3_(&removedAgain, &doc, child, root);
    Require(IsNullId(removedAgain) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850201), "removing non-child reports DomNodeNotFound");

    Require(_ZN3sce3Xml3Dom8Document13recurseDeleteENS1_6NodeIdE(&doc, root) == 0, "recurseDelete on attached root returns 0");
    Require(SameId(Root(&doc), root), "attached root is left alone");
    Require(_ZN3sce3Xml3Dom8Document13recurseDeleteENS1_6NodeIdE(&doc, docRoot) == 0, "recurseDelete on docRoot returns 0");
    Require(!IsNullId(Root(&doc)), "docRoot is left alone");
    Require(_ZN3sce3Xml3Dom8Document13recurseDeleteENS1_6NodeIdE(&doc, child) == 0, "recurseDelete on detached subtree");
    Require(_ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, child) == -1 && Name(&doc, child) == "<null>", "deleted node is invalid");
    Require(_ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, childText) == -1, "deleted descendant is invalid");

    _ZN3sce3Xml13AttributeListD1Ev(&replacementAttrs);
    _ZN3sce3Xml3Dom8NodeListD1Ev(&list);
    _ZN3sce3Xml13AttributeListD1Ev(&attrs);
    _ZN3sce3Xml3Dom8DocumentD1Ev(&doc);
}

void TestSetWritableFromParsedDocument() {
    Fixture f;
    Require(f.Parse("<root a=\"1\"><x>one</x><y b=\"2\"/><?pi proc?><![CDATA[raw]]>tail</root>") == 0, "parse");
    XmlHandle doc{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&doc, &f.builder);
    XmlHandle readOnlyCopy{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1ERKS2_(&readOnlyCopy, &doc);

    const XmlNodeId oldRoot = Root(&doc);
    Require(_ZNK3sce3Xml3Dom8Document10isReadOnlyEv(&doc), "parsed document is read-only");
    const XmlString q = Str("q");
    const XmlString one = Str("1");
    Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&doc, oldRoot, &q, &one) == static_cast<int>(0x80850202), "setAttribute on read-only returns DomReadOnlyError");
    Require(_ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850202), "read-only status recorded");
    Require(_ZN3sce3Xml3Dom8Document12setAttrValueENS1_6NodeIdEPKNS0_6StringES6_(&doc, oldRoot, &q, &one) == static_cast<int>(0x80850202), "setAttrValue on read-only");
    Require(_ZN3sce3Xml3Dom8Document15removeAttributeENS1_6NodeIdEPKNS0_6StringE(&doc, oldRoot, &q) == static_cast<int>(0x80850202), "removeAttribute on read-only");
    Require(_ZN3sce3Xml3Dom8Document16removeAttributesENS1_6NodeIdE(&doc, oldRoot) == static_cast<int>(0x80850202), "removeAttributes on read-only");
    Require(_ZN3sce3Xml3Dom8Document7setTextENS1_6NodeIdEPKNS0_6StringE(&doc, oldRoot, &one) == static_cast<int>(0x80850202), "setText on read-only");
    Require(_ZN3sce3Xml3Dom8Document13recurseDeleteENS1_6NodeIdE(&doc, oldRoot) == static_cast<int>(0x80850202), "recurseDelete on read-only");
    XmlNodeId rejected{};
    _ZN3sce3Xml3Dom8Document13createElementEPKNS0_6StringEPKNS0_13AttributeListES5_(&rejected, &doc, &q, nullptr, nullptr);
    Require(IsNullId(rejected) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850202), "createElement on read-only");
    _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE(&rejected, &doc, &one);
    Require(IsNullId(rejected) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850202), "createTextNode on read-only");
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&rejected, &doc, oldRoot, &q, nullptr, nullptr);
    Require(IsNullId(rejected) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850202), "addElementChild on read-only");
    _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_(&rejected, &doc, oldRoot, XmlNodeId{}, oldRoot);
    Require(IsNullId(rejected) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850202), "insertNode on read-only");
    _ZN3sce3Xml3Dom8Document11removeChildENS1_6NodeIdES3_(&rejected, &doc, oldRoot, oldRoot);
    Require(IsNullId(rejected) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850202), "removeChild on read-only");

    XmlNodeId writableDocRoot{};
    _ZN3sce3Xml3Dom8Document11setWritableEv(&writableDocRoot, &doc);
    Require(!IsNullId(writableDocRoot) && _ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, writableDocRoot) == 0, "setWritable returns docRoot (type 0)");
    Require(!_ZNK3sce3Xml3Dom8Document10isReadOnlyEv(&doc) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == 0, "document is now writable with clear status");
    Require(_ZNK3sce3Xml3Dom8Document10isReadOnlyEv(&readOnlyCopy) && Name(&readOnlyCopy, Root(&readOnlyCopy)) == "root", "prior copy stays read-only and valid");
    Require(_ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(&doc, oldRoot) == -1, "stale read-only NodeId is safely rejected after setWritable");

    const XmlNodeId root = Root(&doc);
    Require(!IsNullId(root) && Name(&doc, root) == "root" && Attribute(&doc, root, "a", 1) == "1", "converted root element and attribute");
    const XmlNodeId x = FirstChild(&doc, root);
    Require(Name(&doc, x) == "x" && TextOf(&doc, FirstChild(&doc, x)) == "one", "converted child x and text");
    const XmlNodeId y = Sibling(&doc, x);
    Require(Name(&doc, y) == "y" && Attribute(&doc, y, "b", 1) == "2", "converted child y and attribute");
    const XmlNodeId pi = Sibling(&doc, y);
    Require(SkippedTextOf(&doc, pi) == "pi proc?>", "converted processing instruction skipped text");
    const XmlNodeId cdata = Sibling(&doc, pi);
    Require(SkippedTextOf(&doc, cdata) == "raw", "converted cdata skipped text");
    const XmlNodeId tail = Sibling(&doc, cdata);
    Require(TextOf(&doc, tail) == "tail", "converted trailing text");

    XmlNodeId writableAgain{};
    _ZN3sce3Xml3Dom8Document11setWritableEv(&writableAgain, &doc);
    Require(SameId(writableAgain, writableDocRoot) && SameId(Root(&doc), root), "setWritable is idempotent and preserves NodeIds");

    const XmlString zStr = Str("z");
    const XmlString ztStr = Str("zt");
    XmlNodeId z{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&z, &doc, root, &zStr, nullptr, &ztStr);
    Require(!IsNullId(z) && TextOf(&doc, FirstChild(&doc, z)) == "zt", "addElementChild works after setWritable");
    Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&doc, root, &q, &one) == 0, "setAttribute works after setWritable");
    Require(Attribute(&doc, root, "q", 1) == "1", "new attribute readable");

    Require(f.Parse("<other/>") == 0, "builder parses another document");
    Require(_ZN3sce3Xml3Dom15DocumentBuilder9terminateEv(&f.builder) == 0, "builder terminates");
    Require(Name(&doc, root) == "root" && Attribute(&doc, root, "q", 1) == "1" && TextOf(&doc, FirstChild(&doc, z)) == "zt", "writable document survives builder re-parse and terminate");

    _ZN3sce3Xml3Dom8DocumentD1Ev(&readOnlyCopy);
    _ZN3sce3Xml3Dom8DocumentD1Ev(&doc);
}

void TestWritableErrorsAndSafety() {
    Fixture f;
    XmlHandle empty{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1Ev(&empty);
    const XmlString name = Str("a");
    const XmlString value = Str("b");
    XmlNodeId id{};
    _ZN3sce3Xml3Dom8Document11setWritableEv(&id, &empty);
    Require(IsNullId(id), "setWritable on empty document returns invalid NodeId");
    Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&empty, XmlNodeId{}, &name, &value) == static_cast<int>(0x80850002), "setAttribute on empty document");
    Require(_ZN3sce3Xml3Dom8Document13recurseDeleteENS1_6NodeIdE(&empty, XmlNodeId{}) == static_cast<int>(0x80850002), "recurseDelete on empty document");

    XmlHandle doc{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1Ev(&doc);
    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&doc, &f.initializer) == 0, "initialize");
    XmlNodeId docRoot{};
    _ZNK3sce3Xml3Dom8Document10getDocRootEv(&docRoot, &doc);

    const XmlString emptyStr{"", 0};
    _ZN3sce3Xml3Dom8Document13createElementEPKNS0_6StringEPKNS0_13AttributeListES5_(&id, &doc, &emptyStr, nullptr, nullptr);
    Require(IsNullId(id) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850003), "empty element name rejected");
    _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE(&id, &doc, &emptyStr);
    Require(IsNullId(id) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850003), "empty text node rejected");

    XmlNodeId root{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&root, &doc, docRoot, &name, nullptr, nullptr);
    XmlNodeId secondRoot{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&secondRoot, &doc, docRoot, &name, nullptr, nullptr);
    Require(IsNullId(secondRoot) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850003), "second root element under docRoot rejected");

    XmlNodeId child{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&child, &doc, root, &value, nullptr, nullptr);
    XmlNodeId cycle{};
    _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_(&cycle, &doc, child, XmlNodeId{}, root);
    Require(IsNullId(cycle) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850003), "inserting ancestor under descendant rejected");

    XmlNodeId textNode{};
    _ZN3sce3Xml3Dom8Document14createTextNodeEPKNS0_6StringE(&textNode, &doc, &value);
    Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&doc, textNode, &name, &value) == static_cast<int>(0x8085020a), "setAttribute on text node returns DomInvalidNodeType");
    XmlNodeId badChild{};
    _ZN3sce3Xml3Dom8Document15addElementChildENS1_6NodeIdEPKNS0_6StringEPKNS0_13AttributeListES6_(&badChild, &doc, textNode, &name, nullptr, nullptr);
    Require(IsNullId(badChild) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x8085020a), "addElementChild on text node returns DomInvalidNodeType");
    Require(_ZN3sce3Xml3Dom8Document7setTextENS1_6NodeIdEPKNS0_6StringE(&doc, docRoot, &value) == static_cast<int>(0x8085020a), "setText on docRoot returns DomInvalidNodeType");

    for (int i = 0; i < 128; ++i) {
        const std::string attrKey = "a" + std::to_string(i);
        const XmlString attrStr{attrKey.c_str(), attrKey.size()};
        Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&doc, child, &attrStr, &value) == 0, "add up to 128 attributes");
    }
    const XmlString overflowKey = Str("overflow");
    Require(_ZN3sce3Xml3Dom8Document12setAttributeENS1_6NodeIdEPKNS0_6StringES6_(&doc, child, &overflowKey, &value) == static_cast<int>(0x80850205), "129th attribute returns DomMaxNumOfAttrError");

    const XmlString hugeUnreadable{reinterpret_cast<const char*>(0x10), std::size_t{1} << 40};
    bool refused = false;
    try {
        _ZN3sce3Xml3Dom8Document13createElementEPKNS0_6StringEPKNS0_13AttributeListES5_(&id, &doc, &hugeUnreadable, nullptr, nullptr);
    } catch (const std::logic_error&) {
        refused = true;
    }
    Require(refused, "unreadable string with huge size throws without huge allocation");

    XmlHandle badAttrs{nullptr};
    _ZN3sce3Xml13AttributeListC1Ev(&badAttrs);
    Require(_ZN3sce3Xml13AttributeList10initializeEPKNS0_11InitializerE(&badAttrs, &f.initializer) == 0, "badAttrs init");
    const XmlString dupVal1 = Str("v1");
    const XmlString dupVal2 = Str("v2");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&badAttrs, &name, &dupVal1) == 0, "add first a");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&badAttrs, &name, &dupVal2) == 0, "add duplicate a");
    Require(_ZN3sce3Xml3Dom8Document16setAttributeListENS1_6NodeIdEPKNS0_13AttributeListE(&doc, root, &badAttrs) == 0, "duplicate attributes deduplicated");
    Require(Attribute(&doc, root, "a", 1) == "v2" && IsNullId(NextAttr(&doc, FirstAttr(&doc, root))), "only one deduplicated attribute remains");
    Require(_ZN3sce3Xml13AttributeList12addAttributeEPKNS0_6StringES4_(&badAttrs, &emptyStr, &dupVal1) == 0, "add empty attr name");
    Require(_ZN3sce3Xml3Dom8Document16setAttributeListENS1_6NodeIdEPKNS0_13AttributeListE(&doc, root, &badAttrs) == static_cast<int>(0x80850003), "empty attr name in list rejected");
    Require(Attribute(&doc, root, "a", 1) == "v2", "failed setAttributeList preserves existing attributes");
    _ZN3sce3Xml13AttributeListD1Ev(&badAttrs);

    GuardedPage page;
    refused = false;
    try {
        _ZN3sce3Xml3Dom8Document11setWritableEv(reinterpret_cast<XmlNodeId*>(page.data + page.size), &doc);
    } catch (const std::logic_error&) {
        refused = true;
    }
    Require(refused, "unwritable NodeId result throws instead of faulting");

    XmlHandle otherDoc{nullptr};
    _ZN3sce3Xml3Dom8DocumentC1Ev(&otherDoc);
    Require(_ZN3sce3Xml3Dom8Document10initializeEPKNS0_11InitializerE(&otherDoc, &f.initializer) == 0, "otherDoc init");
    XmlNodeId foreignElem{};
    _ZN3sce3Xml3Dom8Document13createElementEPKNS0_6StringEPKNS0_13AttributeListES5_(&foreignElem, &otherDoc, &name, nullptr, nullptr);
    XmlNodeId foreignInsert{};
    _ZN3sce3Xml3Dom8Document10insertNodeENS1_6NodeIdES3_S3_(&foreignInsert, &doc, root, XmlNodeId{}, foreignElem);
    Require(IsNullId(foreignInsert) && _ZNK3sce3Xml3Dom8Document9getStatusEv(&doc) == static_cast<int>(0x80850003), "inserting a node from another document is rejected");
    _ZN3sce3Xml3Dom8DocumentD1Ev(&otherDoc);

    _ZN3sce3Xml3Dom8DocumentD1Ev(&doc);
}

}

int main() {
    TestLifecycle();
    TestEmptyDocument();
    TestStatusAndAttributes();
    TestAttributeNameAtPageEnd();
    TestAllocationFailures();
    TestWritableInitializeAndLifecycle();
    TestWritableBuildAndEdit();
    TestSetWritableFromParsedDocument();
    TestWritableErrorsAndSafety();
    std::puts("guest_xml_document: all checks passed");
    return 0;
}
