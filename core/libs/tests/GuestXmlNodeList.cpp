#include "prx/libSceXml/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

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

std::string Name(const XmlHandle* document, XmlNodeId node) {
    XmlString name{};
    _ZNK3sce3Xml3Dom8Document11getNodeNameENS1_6NodeIdE(&name, document, node);
    return name.buffer != nullptr ? std::string(name.buffer, name.size) : std::string("<null>");
}

std::string AttributeName(const XmlHandle* document, XmlNodeId node) {
    XmlString name{};
    _ZNK3sce3Xml3Dom8Document11getAttrNameENS1_6NodeIdE(&name, document, node);
    return name.buffer != nullptr ? std::string(name.buffer, name.size) : std::string("<null>");
}

std::string AttributeValue(const XmlHandle* document, XmlNodeId node) {
    XmlString value{};
    _ZNK3sce3Xml3Dom8Document12getAttrValueENS1_6NodeIdE(&value, document, node);
    return value.buffer != nullptr ? std::string(value.buffer, value.size) : std::string("<null>");
}

bool IsNull(XmlNodeId node) {
    return node.first == nullptr && node.second == nullptr && node.flag == 0;
}

int Start(XmlHandle* list, XmlHandle* initializer) {
    _ZN3sce3Xml3Dom8NodeListC1Ev(list);
    return _ZN3sce3Xml3Dom8NodeList10initializeEPKNS0_11InitializerE(list, initializer);
}

void TestLifecycle() {
    Fixture f;
    XmlHandle list{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Dom8NodeListC1Ev(&list);
    Require(list.impl == nullptr && !_ZNK3sce3Xml3Dom8NodeList11isAvailableEv(&list), "a constructed list is not available");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0 && IsNull(_ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)), "a constructed list is empty");
    Require(_ZN3sce3Xml3Dom8NodeList10insertLastENS1_6NodeIdE(&list, XmlNodeId{}) == static_cast<int>(0x80850002), "inserting into a list that was never initialized");
    Require(_ZN3sce3Xml3Dom8NodeList10initializeEPKNS0_11InitializerE(&list, nullptr) == static_cast<int>(0x80850002), "a null initializer");
    f.Mark();
    Require(_ZN3sce3Xml3Dom8NodeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0, "the list starts");
    Require(f.allocator.log == "a72 ", "the list allocates its block through the initializer's allocator");
    Require(list.impl != nullptr && _ZNK3sce3Xml3Dom8NodeList11isAvailableEv(&list), "an initialized list is available");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0, "a new list is empty");
    Require(_ZN3sce3Xml3Dom8NodeList10initializeEPKNS0_11InitializerE(&list, &f.initializer) == 0x00850003, "initializing a running list");
    Require(_ZN3sce3Xml3Dom8NodeList5clearEv(&list) == 0 && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0, "clearing an empty list");
    f.Mark();
    Require(_ZN3sce3Xml3Dom8NodeList9terminateEv(&list) == 0 && list.impl == nullptr && f.allocator.log == "f-1 ", "terminating the list frees its block");
    Require(_ZN3sce3Xml3Dom8NodeList9terminateEv(&list) == static_cast<int>(0x80850002), "terminating a stopped list");
    Require(!_ZNK3sce3Xml3Dom8NodeList11isAvailableEv(&list) && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0, "a stopped list is empty");
}

void TestItems() {
    Fixture f;
    Require(f.Parse("<r a=\"1\"><b/><c>t</c></r>") == 0, "parse");
    XmlHandle document{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
    const XmlNodeId root = Root(&document);
    XmlNodeId child{};
    _ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE(&child, &document, root);

    XmlHandle list{nullptr};
    Require(Start(&list, &f.initializer) == 0, "the list starts");
    Require(_ZN3sce3Xml3Dom8NodeList10insertLastENS1_6NodeIdE(&list, root) == 0, "insertLast");
    Require(_ZN3sce3Xml3Dom8NodeList11insertFirstENS1_6NodeIdE(&list, child) == 0, "insertFirst");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 2, "two items");
    Require(Name(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)) == "b", "insertFirst puts the item first");
    Require(Name(&document, _ZNK3sce3Xml3Dom8NodeListixEj(&list, 1)) == "r", "insertLast appends and operator[] reads");
    Require(IsNull(_ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 2)) && IsNull(_ZNK3sce3Xml3Dom8NodeListixEj(&list, 2)), "an index past the end");
    Require(_ZN3sce3Xml3Dom8NodeList10removeItemENS1_6NodeIdE(&list, child) == 0, "removing a stored item");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 1 && Name(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)) == "r", "the remaining item");
    Require(_ZN3sce3Xml3Dom8NodeList10removeItemENS1_6NodeIdE(&list, child) == static_cast<int>(0x80850201), "removing an item that is not stored");
    Require(_ZN3sce3Xml3Dom8NodeList11insertFirstENS1_6NodeIdE(&list, child) == 0 && _ZN3sce3Xml3Dom8NodeList5clearEv(&list) == 0, "clearing a list with items");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0 && IsNull(_ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)), "a cleared list is empty");

    _ZN3sce3Xml3Dom8NodeList11insertFirstENS1_6NodeIdE(&list, root);
    _ZN3sce3Xml3Dom8NodeList10insertLastENS1_6NodeIdE(&list, child);
    f.Mark();
    XmlHandle copy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Dom8NodeListC1ERKS2_(&copy, &list);
    Require(f.allocator.log == "a72 ", "a copy allocates its own block");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&copy) == 2 && Name(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&copy, 0)) == "r", "a copy holds the same items");
    Require(_ZN3sce3Xml3Dom8NodeList5clearEv(&copy) == 0 && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 2, "clearing a copy leaves the source alone");
    _ZN3sce3Xml3Dom8NodeListD1Ev(&copy);
    Require(copy.impl == nullptr, "the copy is destroyed");
    XmlHandle emptyCopy{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Dom8NodeListC1ERKS2_(&emptyCopy, &copy);
    Require(emptyCopy.impl == nullptr, "a copy of a destroyed list is empty");
    _ZN3sce3Xml3Dom8NodeListD1Ev(&list);
    _ZN3sce3Xml3Dom8Document9terminateEv(&document);
}

void TestDocumentLists() {
    Fixture f;
    Require(f.Parse("<r a=\"1\"><b/><c>t</c></r>") == 0, "parse");
    XmlHandle document{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
    const XmlNodeId root = Root(&document);

    XmlHandle list{nullptr};
    _ZN3sce3Xml3Dom8NodeListC1Ev(&list);
    Require(_ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE(&document, root, &list) == static_cast<int>(0x80850002), "filling a list that was never initialized");
    Require(Start(&list, &f.initializer) == 0, "the list starts");

    Require(_ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE(&document, root, &list) == 0, "the children");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 2, "two children");
    Require(Name(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)) == "b" && Name(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 1)) == "c", "the children in document order");

    Require(_ZNK3sce3Xml3Dom8Document13getAttributesENS1_6NodeIdEPNS1_8NodeListE(&document, root, &list) == 0, "the attributes");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 1, "one attribute");
    Require(AttributeName(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)) == "a" && AttributeValue(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)) == "1", "the attribute item");

    XmlNodeId child{};
    _ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE(&child, &document, root);
    Require(_ZNK3sce3Xml3Dom8Document13getAttributesENS1_6NodeIdEPNS1_8NodeListE(&document, child, &list) == 0 && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0, "a node without attributes replaces the list with nothing");

    const XmlString c{"c", 1};
    Require(_ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE(&document, root, &c, &list) == 0, "elements by tag name");
    Require(_ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 1 && Name(&document, _ZNK3sce3Xml3Dom8NodeList4itemEj(&list, 0)) == "c", "the matching element");
    const XmlString r{"r", 1};
    Require(_ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE(&document, root, &r, &list) == 0 && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0, "the starting node is not one of its own descendants");
    const XmlString missing{"z", 1};
    Require(_ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE(&document, root, &missing, &list) == 0 && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0, "a name that matches nothing");
    Require(_ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE(&document, root, nullptr, &list) == 0 && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 0, "a null name");

    const XmlString wholeDocument{"b", 1};
    XmlNodeId docRoot{};
    _ZNK3sce3Xml3Dom8Document10getDocRootEv(&docRoot, &document);
    Require(_ZNK3sce3Xml3Dom8Document20getElementsByTagNameENS1_6NodeIdEPKNS0_6StringEPNS1_8NodeListE(&document, docRoot, &wholeDocument, &list) == 0 && _ZNK3sce3Xml3Dom8NodeList9getLengthEv(&list) == 1, "searching from the document node");

    XmlHandle emptyDocument{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Dom8DocumentC1Ev(&emptyDocument);
    Require(_ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE(&emptyDocument, root, &list) == static_cast<int>(0x80850002), "filling from an empty document");
    bool threw = false;
    try {
        _ZNK3sce3Xml3Dom8Document13getChildNodesENS1_6NodeIdEPNS1_8NodeListE(&document, root, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "filling a null list throws");
    _ZN3sce3Xml3Dom8NodeListD1Ev(&list);
    _ZN3sce3Xml3Dom8Document9terminateEv(&document);
}

}

int main() {
    TestLifecycle();
    TestItems();
    TestDocumentLists();
    return 0;
}
