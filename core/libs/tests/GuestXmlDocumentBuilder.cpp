#include "prx/libSceXml/include/Api.hpp"

#include <cstddef>
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
    std::size_t failAt = 0;
    std::size_t allocations = 0;
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

constexpr const char* InitializeLog = "a80 a440 a84 a84 a84 a104 a200 a72 a1240 a96 a2088 a5304 a2088 a1752 a1752 ";
constexpr const char* TerminateLog = "f6 f5 f7 f3 f4 f2 f1 f14 f13 f12 f11 f10 f9 f8 f0 ";

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

    void Mark(std::size_t failAt = 0) {
        allocator.log.clear();
        allocator.first = allocator.blocks.size();
        allocator.failAt = failAt == 0 ? 0 : allocator.allocations + failAt;
    }
};

int Parse(XmlHandle* builder, const char* text, bool last) {
    const XmlString string{text, std::strlen(text)};
    return _ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(builder, &string, last);
}

int Initialize(XmlHandle* builder, const XmlHandle* initializer) {
    return _ZN3sce3Xml3Dom15DocumentBuilder10initializeEPKNS0_11InitializerE(builder, initializer);
}

int Terminate(XmlHandle* builder) {
    return _ZN3sce3Xml3Dom15DocumentBuilder9terminateEv(builder);
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

void TestLifecycle() {
    Fixture f;
    XmlHandle builder{reinterpret_cast<void*>(0x5555)};
    _ZN3sce3Xml3Dom15DocumentBuilderC1Ev(&builder);
    Require(builder.impl == nullptr, "the constructor clears the object");
    XmlHandle never{nullptr};
    f.Mark();
    Require(Initialize(&builder, nullptr) == static_cast<int>(0x80850002), "a null initializer is not initialized");
    Require(Initialize(&builder, &never) == static_cast<int>(0x80850002), "an initializer without an implementation is not initialized");
    Require(Parse(&builder, "<a/>", true) == static_cast<int>(0x80850002), "parse before initialize");
    Require(Terminate(&builder) == static_cast<int>(0x80850002), "terminate before initialize");
    _ZN3sce3Xml3Dom15DocumentBuilderD1Ev(&builder);
    _ZN3sce3Xml3Dom15DocumentBuilder16setResolveEntityEb(&builder, true);
    Require(f.allocator.log.empty(), "nothing is allocated before initialize");
    f.Mark();
    Require(Initialize(&builder, &f.initializer) == 0 && builder.impl != nullptr, "initialize");
    Require(f.allocator.log == InitializeLog, "initialize allocates the 13.42 engine blocks in order");
    f.Mark();
    Require(Initialize(&builder, &f.initializer) == 0x00850003 && f.allocator.log.empty(), "a second initialize is refused");
    const XmlString nullBuffer{nullptr, 4};
    const XmlString empty{"", 0};
    Require(_ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(&builder, nullptr, true) == static_cast<int>(0x80850003), "a null string");
    Require(_ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(&builder, &nullBuffer, true) == static_cast<int>(0x80850003), "a null buffer");
    Require(_ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(&builder, &empty, true) == static_cast<int>(0x80850003), "an empty string");
    f.allocator.first = f.allocator.blocks.size() - 15;
    f.allocator.log.clear();
    Require(Terminate(&builder) == 0 && builder.impl == nullptr, "terminate");
    Require(f.allocator.log == TerminateLog, "terminate frees the engine blocks in the 13.42 order");
    Require(Terminate(&builder) == static_cast<int>(0x80850002), "terminate again");
    f.Mark();
    Require(Initialize(&builder, &f.initializer) == 0, "initialize again");
    f.allocator.log.clear();
    _ZN3sce3Xml3Dom15DocumentBuilderD2Ev(&builder);
    Require(builder.impl == nullptr && f.allocator.log == TerminateLog, "the destructor frees like terminate");
}

void TestStreaming() {
    Fixture f;
    XmlHandle builder{nullptr};
    Require(Initialize(&builder, &f.initializer) == 0, "initialize");
    Require(Parse(&builder, "<a>", false) == 0x00850001, "a chunk that is not the last is in progress");
    Require(Parse(&builder, "</a>", true) == 0, "the last chunk completes the document");
    Require(Parse(&builder, "<a/>", true) == 0, "a parse after the last chunk starts a new document");
    Require(Parse(&builder, "<a/>", false) == 0x00850001, "a complete root in a chunk that is not the last");
    Require(Parse(&builder, "<b/>", true) == static_cast<int>(0x8085000b), "the next chunk continues the same document");
    Require(Parse(&builder, "<a></b>", false) == static_cast<int>(0x8085000a), "an error in a chunk that is not the last");
    Require(Parse(&builder, "<c/>", true) == 0, "a parse after an error starts a new document");
    Require(Parse(&builder, "<a>", false) == 0x00850001, "a document in progress");
    const XmlString empty{"", 0};
    Require(_ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(&builder, &empty, false) == static_cast<int>(0x80850003), "an empty chunk is refused");
    Require(Parse(&builder, "</a>", true) == 0, "an empty chunk keeps the document in progress");
    _ZN3sce3Xml3Dom15DocumentBuilder16setResolveEntityEb(&builder, true);
    Require(Parse(&builder, "<a>&am", false) == 0x00850001, "a reference split across chunks");
    Require(Parse(&builder, "p;</a>", true) == 0, "the split reference resolves");
    Require(Parse(&builder, "<a>&amp", true) == static_cast<int>(0x80850010), "a reference without a semicolon in the last chunk");
    Require(Parse(&builder, "<a x=\"1\" x=\"2\"/>", false) == 0x00850001, "a duplicate attribute stalls inside the tag");
    Require(Throws([&] { Parse(&builder, "<b/>", true); }), "more data after a stall inside a tag is not modelled");
    Require(Terminate(&builder) == 0, "terminate after the stall");
    Require(Initialize(&builder, &f.initializer) == 0, "initialize again");
    Require(Parse(&builder, "<a>", false) == 0x00850001, "a document in progress");
    _ZN3sce3Xml3Dom15DocumentBuilder26setSkipIgnorableWhiteSpaceEb(&builder, true);
    Require(Throws([&] { Parse(&builder, "</a>", true); }), "an option change during a document is not modelled");
    Require(Terminate(&builder) == 0, "terminate");
}

void TestAllocationFailures() {
    Fixture f;
    XmlHandle builder{nullptr};
    f.Mark(1);
    Require(Initialize(&builder, &f.initializer) == static_cast<int>(0x80850001) && builder.impl == nullptr, "the first allocation fails");
    Require(f.allocator.log == "a80 ", "nothing else is allocated");
    f.Mark(12);
    Require(Initialize(&builder, &f.initializer) == static_cast<int>(0x80850001) && builder.impl != nullptr, "allocation 12 fails");
    Require(f.allocator.log == "a80 a440 a84 a84 a84 a104 a200 a72 a1240 a96 a2088 a5304 f10 ", "13.42 frees the 2088-byte block after allocation 12 fails");
    Require(Throws([&] { Parse(&builder, "<a/>", true); }), "parsing after a failed initialize is not measured");
    f.allocator.log.clear();
    Require(Terminate(&builder) == 0, "terminate after the failure");
    Require(f.allocator.log == "f6 f5 f7 f3 f4 f2 f1 f9 f8 f0 ", "terminate frees the remaining blocks in the 13.42 order");
    f.Mark(15);
    Require(Initialize(&builder, &f.initializer) == static_cast<int>(0x80850001), "allocation 15 fails");
    Require(f.allocator.log == std::string(InitializeLog) + "f13 f11 f10 f12 ", "13.42 frees four blocks after allocation 15 fails");
    Require(Terminate(&builder) == 0, "terminate after the failure");
    f.Mark(5);
    Require(Throws([&] { Initialize(&builder, &f.initializer); }), "a failure among allocations 2 to 9 is not modelled");
    Require(builder.impl == nullptr && f.allocator.log == "a80 a440 a84 a84 a84 f3 f2 f1 f0 ", "the throw frees what it allocated");
}

void TestUnmeasuredParse() {
    Fixture f;
    XmlHandle builder{nullptr};
    Require(Initialize(&builder, &f.initializer) == 0, "initialize");
    std::string many = "<r>";
    for (int k = 0; k < 9000; ++k) {
        char name[16];
        std::snprintf(name, sizeof(name), "<n%05d/>", k);
        many += name;
    }
    many += "</r>";
    Require(Throws([&] { Parse(&builder, many.c_str(), true); }), "an element name offset above 0xffff is not measured");
    Require(Throws([&] { Parse(&builder, "<a/>", true); }), "the builder stays stopped after an unmeasured case");
    Require(Terminate(&builder) == 0, "terminate after the unmeasured case");
    Require(Initialize(&builder, &f.initializer) == 0 && Parse(&builder, "<a/>", true) == 0, "a new builder parses");
    Require(Terminate(&builder) == 0, "terminate");
}

}

int main() {
    TestLifecycle();
    TestStreaming();
    TestAllocationFailures();
    TestUnmeasuredParse();
    std::puts("guest_xml_document_builder: all checks passed");
    return 0;
}
