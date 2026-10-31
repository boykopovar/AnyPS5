#include "prx/libSceXml/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
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

}

int main() {
    TestLifecycle();
    TestEmptyDocument();
    TestStatusAndAttributes();
    TestAllocationFailures();
    std::puts("guest_xml_document: all checks passed");
    return 0;
}
