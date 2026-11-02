#include "prx/libSceXml/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::uint8_t GUARD = 0xa5;
constexpr std::uint8_t FILL = 0x5a;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template <std::size_t Size>
struct Guarded {
    std::array<std::uint8_t, 16> before;
    alignas(8) std::array<std::uint8_t, Size> object;
    std::array<std::uint8_t, 16> after;

    explicit Guarded(std::uint8_t fill = FILL) {
        before.fill(GUARD);
        object.fill(fill);
        after.fill(GUARD);
    }

    template <typename T>
    T* As() {
        return reinterpret_cast<T*>(object.data());
    }

    bool GuardsIntact() const {
        for (std::uint8_t b : before) if (b != GUARD) return false;
        for (std::uint8_t b : after) if (b != GUARD) return false;
        return true;
    }

    bool Bytes(std::size_t from, std::size_t to, std::uint8_t value) const {
        for (std::size_t i = from; i < to; ++i) if (object[i] != value) return false;
        return true;
    }

    std::uint64_t Word(std::size_t offset) const {
        std::uint64_t value;
        std::memcpy(&value, object.data() + offset, sizeof(value));
        return value;
    }
};

void* Address(std::uint64_t value) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));
}

void TestHandles() {
    using Constructor = void (APS5_VABI *)(XmlHandle*);
    const Constructor constructors[] = {
        _ZN3sce3Xml4AttrC1Ev, _ZN3sce3Xml4AttrC2Ev, _ZN3sce3Xml13AttributeListC1Ev, _ZN3sce3Xml13AttributeListC2Ev,
        _ZN3sce3Xml3Dom8DocumentC1Ev, _ZN3sce3Xml3Dom8DocumentC2Ev, _ZN3sce3Xml3Dom15DocumentBuilderC1Ev,
        _ZN3sce3Xml3Dom15DocumentBuilderC2Ev, _ZN3sce3Xml11InitializerC1Ev, _ZN3sce3Xml11InitializerC2Ev,
        _ZN3sce3Xml3Dom8NodeListC1Ev, _ZN3sce3Xml3Dom8NodeListC2Ev, _ZN3sce3Xml3Sax6ParserC1Ev, _ZN3sce3Xml3Sax6ParserC2Ev,
    };
    for (Constructor constructor : constructors) {
        Guarded<16> handle;
        constructor(handle.As<XmlHandle>());
        Require(handle.Bytes(0, 8, 0) && handle.Bytes(8, 16, FILL) && handle.GuardsIntact(), "handle constructor clears only its pointer");
    }
}

void TestStrings() {
    static const char text[] = "hello, xml";
    using Default = void (APS5_VABI *)(XmlString*);
    using Counted = void (APS5_VABI *)(XmlString*, const char*, std::size_t);
    using Terminated = void (APS5_VABI *)(XmlString*, const char*);
    using Copy = void (APS5_VABI *)(XmlString*, const XmlString*);
    for (Default construct : {_ZN3sce3Xml6StringC1Ev, _ZN3sce3Xml6StringC2Ev}) {
        Guarded<16> string;
        construct(string.As<XmlString>());
        Require(string.Bytes(0, 16, 0) && string.GuardsIntact(), "String() is empty");
    }
    for (Counted construct : {_ZN3sce3Xml6StringC1EPKcm, _ZN3sce3Xml6StringC2EPKcm}) {
        Guarded<16> string;
        construct(string.As<XmlString>(), text, 3);
        Require(string.As<XmlString>()->buffer == text && string.As<XmlString>()->size == 3 && string.GuardsIntact(),
                "String(const char*, size_t) keeps the explicit length");
        construct(string.As<XmlString>(), nullptr, 0x123456789);
        Require(string.As<XmlString>()->buffer == nullptr && string.As<XmlString>()->size == 0x123456789, "String(const char*, size_t) stores null as given");
    }
    for (Terminated construct : {_ZN3sce3Xml6StringC1EPKc, _ZN3sce3Xml6StringC2EPKc}) {
        Guarded<16> string;
        construct(string.As<XmlString>(), text);
        Require(string.As<XmlString>()->buffer == text && string.As<XmlString>()->size == 10 && string.GuardsIntact(), "String(const char*) uses strlen");
        construct(string.As<XmlString>(), nullptr);
        Require(string.Bytes(0, 16, 0) && string.GuardsIntact(), "String(nullptr) has size 0");
        construct(string.As<XmlString>(), "");
        Require(string.As<XmlString>()->size == 0 && string.As<XmlString>()->buffer != nullptr, "String(\"\") keeps the pointer");
    }
    const XmlString source{text + 1, 7};
    for (Copy copy : {_ZN3sce3Xml6StringC1ERKS1_, _ZN3sce3Xml6StringC2ERKS1_}) {
        Guarded<16> string;
        copy(string.As<XmlString>(), &source);
        Require(string.As<XmlString>()->buffer == text + 1 && string.As<XmlString>()->size == 7 && string.GuardsIntact(), "String copy");
    }
    Guarded<16> assigned;
    Require(_ZN3sce3Xml6StringaSERKS1_(assigned.As<XmlString>(), &source) == assigned.As<XmlString>(), "String::operator= returns this");
    Require(assigned.As<XmlString>()->buffer == text + 1 && assigned.As<XmlString>()->size == 7 && assigned.GuardsIntact(), "String::operator= copies");

    using SimpleDefault = void (APS5_VABI *)(XmlSimpleData*);
    using SimpleCounted = void (APS5_VABI *)(XmlSimpleData*, const char*, std::size_t);
    for (SimpleDefault construct : {_ZN3sce3Xml10SimpleDataC1Ev, _ZN3sce3Xml10SimpleDataC2Ev}) {
        Guarded<16> data;
        construct(data.As<XmlSimpleData>());
        Require(data.Bytes(0, 16, 0) && data.GuardsIntact(), "SimpleData() is empty");
    }
    for (SimpleCounted construct : {_ZN3sce3Xml10SimpleDataC1EPKcm, _ZN3sce3Xml10SimpleDataC2EPKcm}) {
        Guarded<16> data;
        construct(data.As<XmlSimpleData>(), text, 4);
        Require(data.As<XmlSimpleData>()->data == text && data.As<XmlSimpleData>()->size == 4 && data.GuardsIntact(), "SimpleData(const char*, size_t)");
    }
}

void TestSerializeParameterAndAllocator() {
    using Default = void (APS5_VABI *)(XmlSerializeParameter*);
    for (Default construct : {_ZN3sce3Xml18SerializeParameterC1Ev, _ZN3sce3Xml18SerializeParameterC2Ev}) {
        Guarded<24> parameter;
        construct(parameter.As<XmlSerializeParameter>());
        Require(parameter.Bytes(0, 20, 0) && parameter.Bytes(20, 24, FILL) && parameter.GuardsIntact(), "SerializeParameter clears 20 bytes");
    }
    using Destructor = void (APS5_VABI *)(void*);
    for (Destructor destroy : {_ZN3sce3Xml12MemAllocatorD1Ev, _ZN3sce3Xml12MemAllocatorD2Ev}) {
        Guarded<16> allocator;
        destroy(allocator.object.data());
        Require(allocator.Bytes(0, 16, FILL) && allocator.GuardsIntact(), "MemAllocator destructor writes nothing");
    }
}

XmlNodeId NodeIdValue(std::uint64_t first, std::uint64_t second, std::uint8_t flag, std::uint8_t padding) {
    XmlNodeId value;
    std::memset(&value, padding, sizeof(value));
    value.first = Address(first);
    value.second = Address(second);
    value.flag = flag;
    return value;
}

void TestNodeId() {
    using Default = void (APS5_VABI *)(XmlNodeId*);
    using Fields = void (APS5_VABI *)(XmlNodeId*, void*, void*, std::uint8_t);
    for (Default construct : {_ZN3sce3Xml3Dom6NodeIdC1Ev, _ZN3sce3Xml3Dom6NodeIdC2Ev}) {
        Guarded<24> id;
        construct(id.As<XmlNodeId>());
        Require(id.Bytes(0, 17, 0) && id.Bytes(17, 24, FILL) && id.GuardsIntact(), "NodeId() leaves its padding");
    }
    for (Fields construct : {_ZN3sce3Xml3Dom6NodeIdC1EPvS3_b, _ZN3sce3Xml3Dom6NodeIdC2EPvS3_b}) {
        Guarded<24> id;
        construct(id.As<XmlNodeId>(), Address(0x1122334455667788), Address(0x8877665544332211), 0xc3);
        Require(id.Word(0) == 0x1122334455667788 && id.Word(8) == 0x8877665544332211 && id.object[16] == 0xc3 && id.Bytes(17, 24, FILL) && id.GuardsIntact(),
                "NodeId(void*, void*, bool) keeps a nonzero flag byte and the padding");
    }

    Guarded<24> self;
    const XmlNodeId value = NodeIdValue(0x0102030405060708, 0x1112131415161718, 0x81, 0xee);
    const XmlNodeId returned = _ZN3sce3Xml3Dom6NodeIdaSES2_(self.As<XmlNodeId>(), value);
    Require(self.Word(0) == 0x0102030405060708 && self.Word(8) == 0x1112131415161718 && self.object[16] == 0x81 && self.Bytes(17, 24, FILL) && self.GuardsIntact(),
            "NodeId::operator= assigns the fields by value and leaves the padding");
    Require(std::memcmp(&returned, self.object.data(), sizeof(returned)) == 0, "NodeId::operator= returns a copy of this, padding included");

    const XmlNodeId base = NodeIdValue(0xaaaa, 0xbbbb, 0x80, 0x00);
    const XmlNodeId paddingOnly = NodeIdValue(0xaaaa, 0xbbbb, 0x80, 0xff);
    const XmlNodeId otherFlag = NodeIdValue(0xaaaa, 0xbbbb, 0x81, 0x00);
    const XmlNodeId otherFirst = NodeIdValue(0xaaab, 0xbbbb, 0x80, 0x00);
    const XmlNodeId otherSecond = NodeIdValue(0xaaaa, 0xbbbc, 0x80, 0x00);
    Require(_ZN3sce3Xml3Dom6NodeIdeqES2_(&base, paddingOnly) && !_ZN3sce3Xml3Dom6NodeIdneES2_(&base, paddingOnly), "NodeId comparison ignores padding");
    for (const XmlNodeId* other : {&otherFlag, &otherFirst, &otherSecond}) {
        Require(!_ZN3sce3Xml3Dom6NodeIdeqES2_(&base, *other) && _ZN3sce3Xml3Dom6NodeIdneES2_(&base, *other), "NodeId comparison uses every field");
    }
    XmlNodeId any = NodeIdValue(0x1234, 0x5678, 1, 0);
    Require(_ZN3sce3Xml3Dom6NodeIdcvPvEv(&any) == nullptr, "NodeId::operator void* is null");
}

void TestNode() {
    std::array<std::uint8_t, 64> pattern{};
    for (std::size_t i = 0; i < pattern.size(); ++i) pattern[i] = static_cast<std::uint8_t>(0x40 + i);
    const auto* source = reinterpret_cast<const XmlNode*>(pattern.data());
    auto copied = [&](const Guarded<64>& node) {
        return std::memcmp(node.object.data(), pattern.data(), 17) == 0 && node.Bytes(17, 24, FILL) &&
               std::memcmp(node.object.data() + 24, pattern.data() + 24, 40) == 0 && node.GuardsIntact();
    };
    using Copy = void (APS5_VABI *)(XmlNode*, const XmlNode*);
    for (Copy copy : {_ZN3sce3Xml3Dom4NodeC1ERKS2_, _ZN3sce3Xml3Dom4NodeC2ERKS2_}) {
        Guarded<64> node;
        copy(node.As<XmlNode>(), source);
        Require(copied(node), "Node copy keeps its NodeId padding and copies the rest");
    }
    Guarded<64> node;
    Require(_ZN3sce3Xml3Dom4NodeaSERKS2_(node.As<XmlNode>(), source) == node.As<XmlNode>(), "Node::operator= returns this");
    Require(copied(node), "Node::operator= copies like the copy constructor");
    Guarded<64> same(0x33);
    Require(_ZN3sce3Xml3Dom4NodeaSERKS2_(same.As<XmlNode>(), same.As<XmlNode>()) == same.As<XmlNode>() && same.Bytes(0, 64, 0x33) && same.GuardsIntact(),
            "Node self-assignment changes nothing");

    const std::uint64_t pointers[] = {0, 0x1, 0x0100000000000000};
    for (std::uint64_t document : pointers) {
        for (std::uint64_t data : pointers) {
            Guarded<64> available(0);
            std::memcpy(available.object.data() + 0x18, &document, 8);
            std::memcpy(available.object.data() + 0x20, &data, 8);
            Require(_ZNK3sce3Xml3Dom4Node11isAvailableEv(available.As<XmlNode>()) == (document != 0 && data != 0), "Node::isAvailable needs both pointers");
        }
    }
}

}

int main() {
    TestHandles();
    TestStrings();
    TestSerializeParameterAndAllocator();
    TestNodeId();
    TestNode();
    std::puts("guest_xml_value_types: all checks passed");
    return 0;
}
