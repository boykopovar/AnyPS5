#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/include/ResourceLoadStatisticsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using NKWebKit::WebCoreAbi::KeyedDecoder;
using NKWebKit::WebCoreAbi::KeyedDecoderVtable;
using NKWebKit::WebCoreAbi::KeyedEncoder;
using NKWebKit::WebCoreAbi::KeyedEncoderVtable;
using NKWebKit::WebCoreAbi::RegistrableDomain;
using NKWebKit::WebCoreAbi::ResourceLoadStatistics;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit ResourceLoadStatistics check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static String MakeGuestString(const char* literal) {
    unsigned length = 0;
    while (literal[length]) ++length;
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(literal), length);
}

static std::string ImplText(const StringImpl* impl) {
    if (!impl) return {};
    auto view = NKWebKit::WtfAbi::ViewOf(impl);
    std::string text;
    for (unsigned index = 0; index < view.length; ++index) {
        char16_t unit = view.is8Bit ? static_cast<const unsigned char*>(view.data)[index] : static_cast<const char16_t*>(view.data)[index];
        text.push_back(static_cast<char>(unit));
    }
    return text;
}

static constexpr std::uintptr_t DeletedImplValue = ~static_cast<std::uintptr_t>(0);

static bool SetIsEmpty(const void* set) {
    auto* table = static_cast<const String*>(set);
    return !table || reinterpret_cast<const unsigned*>(table)[-3] == 0;
}

static std::vector<std::string> SetContents(const void* set) {
    std::vector<std::string> contents;
    auto* table = static_cast<const String*>(set);
    if (!table) return contents;
    unsigned size = reinterpret_cast<const unsigned*>(table)[-1];
    for (unsigned index = 0; index < size; ++index) {
        const StringImpl* impl = table[index].impl;
        if (!impl || impl == reinterpret_cast<const StringImpl*>(DeletedImplValue)) continue;
        contents.push_back(ImplText(impl));
    }
    return contents;
}

static bool SetHasDomain(const void* set, const char* domain) {
    for (const std::string& entry : SetContents(set)) {
        std::string text(domain);
        if (entry.size() != text.size()) continue;
        bool equal = true;
        for (std::size_t index = 0; index < text.size(); ++index) {
            char a = entry[index] >= 'A' && entry[index] <= 'Z' ? static_cast<char>(entry[index] + 0x20) : entry[index];
            char b = text[index] >= 'A' && text[index] <= 'Z' ? static_cast<char>(text[index] + 0x20) : text[index];
            if (a != b) {
                equal = false;
                break;
            }
        }
        if (equal) return true;
    }
    return false;
}

static const StringImpl* FirstDomainImpl(const void* set) {
    auto* table = static_cast<const String*>(set);
    if (!table) return nullptr;
    unsigned size = reinterpret_cast<const unsigned*>(table)[-1];
    for (unsigned index = 0; index < size; ++index) {
        const StringImpl* impl = table[index].impl;
        if (impl && impl != reinterpret_cast<const StringImpl*>(DeletedImplValue)) return impl;
    }
    return nullptr;
}

static unsigned SetTableSize(const void* set) {
    auto* table = static_cast<const String*>(set);
    return table ? reinterpret_cast<const unsigned*>(table)[-1] : 0;
}

struct MockNode {
    enum class Kind { Bool, UInt32, Double, String, Dict, Array };
    Kind kind{Kind::Dict};
    bool boolValue{false};
    std::uint32_t uint32Value{0};
    double doubleValue{0.0};
    std::string stringValue;
    std::unique_ptr<std::map<std::string, MockNode>> dict{std::make_unique<std::map<std::string, MockNode>>()};
    std::vector<std::unique_ptr<std::map<std::string, MockNode>>> array;
};

using MockDict = std::map<std::string, MockNode>;

static MockNode& Put(MockDict& dict, const char* key, bool value) {
    MockNode& node = dict[key];
    node.kind = MockNode::Kind::Bool;
    node.boolValue = value;
    return node;
}

static MockNode& Put(MockDict& dict, const char* key, std::uint32_t value) {
    MockNode& node = dict[key];
    node.kind = MockNode::Kind::UInt32;
    node.uint32Value = value;
    return node;
}

static MockNode& Put(MockDict& dict, const char* key, double value) {
    MockNode& node = dict[key];
    node.kind = MockNode::Kind::Double;
    node.doubleValue = value;
    return node;
}

static MockNode& Put(MockDict& dict, const char* key, const char* value) {
    MockNode& node = dict[key];
    node.kind = MockNode::Kind::String;
    node.stringValue = value;
    return node;
}

static std::vector<std::unique_ptr<MockDict>>& PutArray(MockDict& dict, const char* key) {
    MockNode& node = dict[key];
    node.kind = MockNode::Kind::Array;
    return node.array;
}

static MockDict& AddArrayElement(MockDict& dict, const char* key) {
    auto& array = PutArray(dict, key);
    array.push_back(std::make_unique<MockDict>());
    return *array.back();
}

struct MockDecoderState {
    MockDict* root{nullptr};
    std::vector<MockDict*> dictionaries;
    std::vector<std::vector<std::unique_ptr<MockDict>>*> arrays;
    std::vector<unsigned> indices;
};

struct MockDecoderObject {
    const KeyedDecoderVtable* vtable;
    MockDecoderState state;
};

static bool APS5_VABI MockDecodeBool(void* self, const String* key, bool* result) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    auto found = state.dictionaries.back()->find(ImplText(key->impl));
    if (found == state.dictionaries.back()->end() || found->second.kind != MockNode::Kind::Bool) return false;
    *result = found->second.boolValue;
    return true;
}

static bool APS5_VABI MockDecodeUInt32(void* self, const String* key, std::uint32_t* result) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    auto found = state.dictionaries.back()->find(ImplText(key->impl));
    if (found == state.dictionaries.back()->end() || found->second.kind != MockNode::Kind::UInt32) return false;
    *result = found->second.uint32Value;
    return true;
}

static bool APS5_VABI MockDecodeDouble(void* self, const String* key, double* result) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    auto found = state.dictionaries.back()->find(ImplText(key->impl));
    if (found == state.dictionaries.back()->end() || found->second.kind != MockNode::Kind::Double) return false;
    *result = found->second.doubleValue;
    return true;
}

static bool APS5_VABI MockDecodeString(void* self, const String* key, String* result) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    auto found = state.dictionaries.back()->find(ImplText(key->impl));
    if (found == state.dictionaries.back()->end() || found->second.kind != MockNode::Kind::String) return false;
    *result = MakeGuestString(found->second.stringValue.c_str());
    return true;
}

static bool APS5_VABI MockBeginObject(void* self, const String* key) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    auto found = state.dictionaries.back()->find(ImplText(key->impl));
    if (found == state.dictionaries.back()->end() || found->second.kind != MockNode::Kind::Dict) return false;
    state.dictionaries.push_back(found->second.dict.get());
    return true;
}

static void APS5_VABI MockEndObject(void* self) {
    static_cast<MockDecoderObject*>(self)->state.dictionaries.pop_back();
}

static bool APS5_VABI MockBeginArray(void* self, const String* key) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    auto found = state.dictionaries.back()->find(ImplText(key->impl));
    if (found == state.dictionaries.back()->end() || found->second.kind != MockNode::Kind::Array) return false;
    state.arrays.push_back(&found->second.array);
    state.indices.push_back(0);
    return true;
}

static bool APS5_VABI MockBeginArrayElement(void* self) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    if (state.indices.back() >= state.arrays.back()->size()) return false;
    state.dictionaries.push_back(state.arrays.back()->at(state.indices.back()++).get());
    return true;
}

static void APS5_VABI MockEndArrayElement(void* self) {
    static_cast<MockDecoderObject*>(self)->state.dictionaries.pop_back();
}

static void APS5_VABI MockEndArray(void* self) {
    auto& state = static_cast<MockDecoderObject*>(self)->state;
    state.arrays.pop_back();
    state.indices.pop_back();
}

static KeyedDecoderVtable MockDecoderVtable() {
    KeyedDecoderVtable vtable{};
    vtable.decodeBool = &MockDecodeBool;
    vtable.decodeUInt32 = &MockDecodeUInt32;
    vtable.decodeDouble = &MockDecodeDouble;
    vtable.decodeString = &MockDecodeString;
    vtable.beginObject = &MockBeginObject;
    vtable.endObject = &MockEndObject;
    vtable.beginArray = &MockBeginArray;
    vtable.beginArrayElement = &MockBeginArrayElement;
    vtable.endArrayElement = &MockEndArrayElement;
    vtable.endArray = &MockEndArray;
    return vtable;
}

static KeyedDecoder* MakeDecoder(MockDecoderObject& object, MockDict& root) {
    object.state.root = &root;
    object.state.dictionaries.push_back(&root);
    static const KeyedDecoderVtable vtable = MockDecoderVtable();
    object.vtable = &vtable;
    return reinterpret_cast<KeyedDecoder*>(&object);
}

struct RecordedCall {
    enum class Kind { Bool, UInt32, Double, String, BeginArray, BeginArrayElement, EndArrayElement, EndArray };
    Kind kind;
    std::string key;
    bool boolValue{false};
    std::uint32_t uint32Value{0};
    double doubleValue{0.0};
    std::string stringValue;
};

static std::vector<RecordedCall>* RecordedCalls = nullptr;

static void APS5_VABI MockEncodeBool(void*, const String* key, bool value) {
    RecordedCall call{RecordedCall::Kind::Bool, ImplText(key->impl)};
    call.boolValue = value;
    RecordedCalls->push_back(call);
}

static void APS5_VABI MockEncodeUInt32(void*, const String* key, std::uint32_t value) {
    RecordedCall call{RecordedCall::Kind::UInt32, ImplText(key->impl)};
    call.uint32Value = value;
    RecordedCalls->push_back(call);
}

static void APS5_VABI MockEncodeDouble(void*, const String* key, double value) {
    RecordedCall call{RecordedCall::Kind::Double, ImplText(key->impl)};
    call.doubleValue = value;
    RecordedCalls->push_back(call);
}

static void APS5_VABI MockEncodeString(void*, const String* key, const String* value) {
    RecordedCall call{RecordedCall::Kind::String, ImplText(key->impl)};
    call.stringValue = ImplText(value->impl);
    RecordedCalls->push_back(call);
}

static void APS5_VABI MockBeginArrayEncode(void*, const String* key) {
    RecordedCall call{RecordedCall::Kind::BeginArray, ImplText(key->impl)};
    RecordedCalls->push_back(call);
}

static void APS5_VABI MockBeginArrayElementEncode(void*) {
    RecordedCall call{RecordedCall::Kind::BeginArrayElement};
    RecordedCalls->push_back(call);
}

static void APS5_VABI MockEndArrayElementEncode(void*) {
    RecordedCall call{RecordedCall::Kind::EndArrayElement};
    RecordedCalls->push_back(call);
}

static void APS5_VABI MockEndArrayEncode(void*) {
    RecordedCall call{RecordedCall::Kind::EndArray};
    RecordedCalls->push_back(call);
}

static KeyedEncoder MakeEncoder() {
    static const KeyedEncoderVtable vtable = [] {
        KeyedEncoderVtable table{};
        table.encodeBool = &MockEncodeBool;
        table.encodeUInt32 = &MockEncodeUInt32;
        table.encodeDouble = &MockEncodeDouble;
        table.encodeString = &MockEncodeString;
        table.beginArray = &MockBeginArrayEncode;
        table.beginArrayElement = &MockBeginArrayElementEncode;
        table.endArrayElement = &MockEndArrayElementEncode;
        table.endArray = &MockEndArrayEncode;
        return table;
    }();
    return KeyedEncoder{&vtable};
}

static std::string MergeText(const std::vector<std::string>& parts) {
    std::string text;
    for (const std::string& part : parts) text += part;
    return text;
}

template<typename Function, typename... Arguments>
static bool ThrowsInvalidArgument(Function function, Arguments&&... arguments) {
    try {
        function(arguments...);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestMergeScalars() {
    ResourceLoadStatistics self{};
    ResourceLoadStatistics other{};

    self.lastSeen = WTF::WallTime{100.0};
    other.lastSeen = WTF::WallTime{200.0};
    self.hadUserInteraction = true;
    self.mostRecentUserInteractionTime = WTF::WallTime{500.0};
    other.hadUserInteraction = true;
    other.mostRecentUserInteractionTime = WTF::WallTime{600.0};
    other.grandfathered = true;
    other.gotLinkDecorationFromPrevalentResource = true;
    other.isPrevalentResource = true;
    other.isVeryPrevalentResource = true;
    self.dataRecordsRemoved = 3;
    other.dataRecordsRemoved = 9;

    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&self, &other);
    Require(self.lastSeen.m_value == 200.0);
    Require(self.hadUserInteraction);
    Require(self.mostRecentUserInteractionTime.m_value == 600.0);
    Require(self.grandfathered);
    Require(self.gotLinkDecorationFromPrevalentResource);
    Require(self.isPrevalentResource);
    Require(self.isVeryPrevalentResource);
    Require(self.dataRecordsRemoved == 9);

    ResourceLoadStatistics resetSelf{};
    ResourceLoadStatistics resetOther{};
    resetSelf.lastSeen = WTF::WallTime{100.0};
    resetSelf.hadUserInteraction = true;
    resetSelf.mostRecentUserInteractionTime = WTF::WallTime{500.0};
    resetOther.lastSeen = WTF::WallTime{50.0};
    resetOther.dataRecordsRemoved = 1;
    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&resetSelf, &resetOther);
    Require(!resetSelf.hadUserInteraction);
    Require(resetSelf.mostRecentUserInteractionTime.m_value == 0.0);
    Require(resetSelf.lastSeen.m_value == 100.0);
    Require(resetSelf.dataRecordsRemoved == 1);

    ResourceLoadStatistics keepSelf{};
    ResourceLoadStatistics keepOther{};
    keepSelf.lastSeen = WTF::WallTime{40.0};
    keepSelf.hadUserInteraction = false;
    keepSelf.mostRecentUserInteractionTime = WTF::WallTime{10.0};
    keepOther.hadUserInteraction = true;
    keepOther.mostRecentUserInteractionTime = WTF::WallTime{20.0};
    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&keepSelf, &keepOther);
    Require(keepSelf.hadUserInteraction);
    Require(keepSelf.mostRecentUserInteractionTime.m_value == 20.0);

    ResourceLoadStatistics olderSelf{};
    ResourceLoadStatistics olderOther{};
    olderSelf.hadUserInteraction = true;
    olderSelf.mostRecentUserInteractionTime = WTF::WallTime{800.0};
    olderOther.hadUserInteraction = true;
    olderOther.mostRecentUserInteractionTime = WTF::WallTime{700.0};
    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&olderSelf, &olderOther);
    Require(olderSelf.mostRecentUserInteractionTime.m_value == 800.0);

    ResourceLoadStatistics noopSelf{};
    ResourceLoadStatistics noopOther{};
    noopSelf.hadUserInteraction = true;
    noopSelf.mostRecentUserInteractionTime = WTF::WallTime{300.0};
    noopOther.hadUserInteraction = false;
    noopOther.mostRecentUserInteractionTime = WTF::WallTime{100.0};
    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&noopSelf, &noopOther);
    Require(noopSelf.hadUserInteraction);
    Require(noopSelf.mostRecentUserInteractionTime.m_value == 300.0);
}

static MockDict VersionFifteenPayload() {
    MockDict root;
    Put(root, "PrevalentResourceDomain", "example.com");
    Put(root, "hadUserInteraction", true);
    AddArrayElement(root, "storageAccessUnderTopFrameDomains");
    {
        auto& array = PutArray(root, "storageAccessUnderTopFrameDomains");
        Put(*array.front(), "domain", "a.com");
    }
    AddArrayElement(root, "topFrameUniqueRedirectsFrom");
    {
        auto& array = PutArray(root, "topFrameUniqueRedirectsFrom");
        Put(*array.front(), "domain", "b.com");
    }
    Put(root, "gotLinkDecorationFromPrevalentResource", true);
    {
        auto& element = AddArrayElement(root, "topFrameLoadedThirdPartyScripts");
        Put(element, "origin", "c.com");
        Put(element, "count", std::uint32_t{2});
    }
    AddArrayElement(root, "subframeUnderTopFrameDomains");
    {
        auto& array = PutArray(root, "subframeUnderTopFrameDomains");
        Put(*array.front(), "domain", "d.com");
    }
    AddArrayElement(root, "subresourceUnderTopFrameDomains");
    {
        auto& array = PutArray(root, "subresourceUnderTopFrameDomains");
        Put(*array.front(), "domain", "e.com");
    }
    Put(root, "isPrevalentResource", true);
    Put(root, "isVeryPrevalentResource", false);
    Put(root, "dataRecordsRemoved", std::uint32_t{42});
    Put(root, "mostRecentUserInteraction", 0.0);
    Put(root, "grandfathered", false);
    Put(root, "lastSeen", 1000.5);
    Put(root, "timesAccessedAsFirstPartyDueToUserInteraction", std::uint32_t{7});
    Put(root, "timesAccessedAsFirstPartyDueToStorageAccessAPI", std::uint32_t{3});
    return root;
}

static void TestDecodeVersionSeventeen() {
    MockDict root = VersionFifteenPayload();
    MockDecoderObject decoderObject;
    KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
    ResourceLoadStatistics statistics{};
    bool result = _ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 17);
    Require(result);
    Require(ImplText(statistics.registrableDomain.string.impl) == "example.com");
    Require(statistics.registrableDomain.string.impl->refCount == 2);
    Require(statistics.hadUserInteraction);
    Require(statistics.lastSeen.m_value == 1000.5);
    Require(statistics.mostRecentUserInteractionTime.m_value == 0.0);
    Require(!statistics.grandfathered);
    Require(!SetIsEmpty(statistics.storageAccessUnderTopFrameDomains));
    Require(SetHasDomain(statistics.storageAccessUnderTopFrameDomains, "a.com"));
    Require(SetIsEmpty(statistics.topFrameUniqueRedirectsTo));
    Require(SetHasDomain(statistics.topFrameUniqueRedirectsFrom, "b.com"));
    Require(statistics.gotLinkDecorationFromPrevalentResource);
    Require(SetHasDomain(statistics.topFrameLoadedThirdPartyScripts, "c.com"));
    Require(SetHasDomain(statistics.subframeUnderTopFrameDomains, "d.com"));
    Require(SetHasDomain(statistics.subresourceUnderTopFrameDomains, "e.com"));
    Require(SetIsEmpty(statistics.subresourceUniqueRedirectsTo));
    Require(SetIsEmpty(statistics.subresourceUniqueRedirectsFrom));
    Require(statistics.isPrevalentResource);
    Require(!statistics.isVeryPrevalentResource);
    Require(statistics.dataRecordsRemoved == 42);
    Require(statistics.timesAccessedAsFirstPartyDueToUserInteraction == 7);
    Require(statistics.timesAccessedAsFirstPartyDueToStorageAccessAPI == 3);
}

static void TestDecodeVersionThirteen() {
    MockDict root;
    Put(root, "PrevalentResourceOrigin", "old.com");
    Put(root, "hadUserInteraction", false);
    {
        auto& element = AddArrayElement(root, "storageAccessUnderTopFrameOrigins");
        Put(element, "origin", "x.com");
    }
    {
        auto& element = AddArrayElement(root, "topFrameUniqueRedirectsTo");
        Put(element, "origin", "r1.com");
        Put(element, "count", std::uint32_t{2});
    }
    {
        auto& element = AddArrayElement(root, "subresourceUnderTopFrameOrigins");
        Put(element, "origin", "s1.com");
        Put(element, "count", std::uint32_t{5});
    }
    {
        auto& element = AddArrayElement(root, "subresourceUniqueRedirectsTo");
        Put(element, "origin", "s2.com");
        Put(element, "count", std::uint32_t{1});
    }
    {
        auto& element = AddArrayElement(root, "subresourceUniqueRedirectsFrom");
        Put(element, "origin", "s3.com");
        Put(element, "count", std::uint32_t{1});
    }
    Put(root, "isPrevalentResource", true);
    Put(root, "isVeryPrevalentResource", true);
    Put(root, "dataRecordsRemoved", std::uint32_t{1});
    Put(root, "mostRecentUserInteraction", 5.0);
    Put(root, "grandfathered", true);
    Put(root, "lastSeen", 2.0);
    Put(root, "timesAccessedAsFirstPartyDueToUserInteraction", std::uint32_t{4});
    Put(root, "timesAccessedAsFirstPartyDueToStorageAccessAPI", std::uint32_t{6});

    MockDecoderObject decoderObject;
    KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
    ResourceLoadStatistics statistics{};
    statistics.isPrevalentResource = true;
    statistics.isVeryPrevalentResource = true;
    bool result = _ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 13);
    Require(result);
    Require(ImplText(statistics.registrableDomain.string.impl) == "old.com");
    Require(!statistics.hadUserInteraction);
    Require(SetHasDomain(statistics.storageAccessUnderTopFrameDomains, "x.com"));
    Require(SetHasDomain(statistics.topFrameUniqueRedirectsTo, "r1.com"));
    Require(SetIsEmpty(statistics.topFrameUniqueRedirectsFrom));
    Require(!statistics.gotLinkDecorationFromPrevalentResource);
    Require(SetIsEmpty(statistics.topFrameLoadedThirdPartyScripts));
    Require(SetIsEmpty(statistics.subframeUnderTopFrameDomains));
    Require(SetHasDomain(statistics.subresourceUnderTopFrameDomains, "s1.com"));
    Require(SetHasDomain(statistics.subresourceUniqueRedirectsTo, "s2.com"));
    Require(SetHasDomain(statistics.subresourceUniqueRedirectsFrom, "s3.com"));
    Require(!statistics.isPrevalentResource);
    Require(!statistics.isVeryPrevalentResource);
    Require(statistics.dataRecordsRemoved == 1);
    Require(statistics.mostRecentUserInteractionTime.m_value == 5.0);
    Require(statistics.grandfathered);
    Require(statistics.lastSeen.m_value == 2.0);
    Require(statistics.timesAccessedAsFirstPartyDueToUserInteraction == 4);
    Require(statistics.timesAccessedAsFirstPartyDueToStorageAccessAPI == 6);
}

static void TestDecodeVersionFourteen() {
    MockDict root;
    Put(root, "PrevalentResourceOrigin", "v14.com");
    Put(root, "hadUserInteraction", true);
    {
        auto& element = AddArrayElement(root, "storageAccessUnderTopFrameOrigins");
        Put(element, "origin", "s14.com");
    }
    {
        auto& element = AddArrayElement(root, "topFrameUniqueRedirectsTo");
        Put(element, "origin", "r14.com");
        Put(element, "count", std::uint32_t{3});
    }
    {
        auto& element = AddArrayElement(root, "subframeUnderTopFrameOrigins");
        Put(element, "origin", "f14.com");
        Put(element, "count", std::uint32_t{2});
    }
    {
        auto& element = AddArrayElement(root, "subresourceUniqueRedirectsFrom");
        Put(element, "origin", "g14.com");
        Put(element, "count", std::uint32_t{1});
    }
    Put(root, "isPrevalentResource", true);
    Put(root, "isVeryPrevalentResource", true);
    Put(root, "dataRecordsRemoved", std::uint32_t{5});
    Put(root, "mostRecentUserInteraction", 7.0);
    Put(root, "grandfathered", true);
    Put(root, "lastSeen", 3.0);
    Put(root, "timesAccessedAsFirstPartyDueToUserInteraction", std::uint32_t{1});
    Put(root, "timesAccessedAsFirstPartyDueToStorageAccessAPI", std::uint32_t{2});

    MockDecoderObject decoderObject;
    KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
    ResourceLoadStatistics statistics{};
    Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 14));
    Require(ImplText(statistics.registrableDomain.string.impl) == "v14.com");
    Require(statistics.hadUserInteraction);
    Require(SetHasDomain(statistics.storageAccessUnderTopFrameDomains, "s14.com"));
    Require(SetHasDomain(statistics.topFrameUniqueRedirectsTo, "r14.com"));
    Require(SetIsEmpty(statistics.topFrameUniqueRedirectsFrom));
    Require(!statistics.gotLinkDecorationFromPrevalentResource);
    Require(SetIsEmpty(statistics.topFrameLoadedThirdPartyScripts));
    Require(SetHasDomain(statistics.subframeUnderTopFrameDomains, "f14.com"));
    Require(SetIsEmpty(statistics.subresourceUnderTopFrameDomains));
    Require(SetIsEmpty(statistics.subresourceUniqueRedirectsTo));
    Require(SetHasDomain(statistics.subresourceUniqueRedirectsFrom, "g14.com"));
    Require(statistics.isPrevalentResource);
    Require(statistics.isVeryPrevalentResource);
    Require(statistics.dataRecordsRemoved == 5);
    Require(statistics.mostRecentUserInteractionTime.m_value == 7.0);
    Require(statistics.grandfathered);
    Require(statistics.lastSeen.m_value == 3.0);
    Require(statistics.timesAccessedAsFirstPartyDueToUserInteraction == 1);
    Require(statistics.timesAccessedAsFirstPartyDueToStorageAccessAPI == 2);
}

static void TestDecodeVersionTen() {
    MockDict root;
    Put(root, "PrevalentResourceOrigin", "ancient.com");
    Put(root, "hadUserInteraction", true);
    {
        auto& element = AddArrayElement(root, "storageAccessUnderTopFrameOrigins");
        Put(element, "origin", "y.com");
    }
    {
        auto& element = AddArrayElement(root, "subresourceUnderTopFrameOrigins");
        Put(element, "origin", "z.com");
        Put(element, "count", std::uint32_t{3});
    }
    {
        auto& element = AddArrayElement(root, "subresourceUniqueRedirectsTo");
        Put(element, "origin", "w.com");
        Put(element, "count", std::uint32_t{1});
    }
    Put(root, "isPrevalentResource", true);
    Put(root, "dataRecordsRemoved", std::uint32_t{8});
    Put(root, "mostRecentUserInteraction", 9.0);
    Put(root, "grandfathered", true);
    Put(root, "lastSeen", 4.0);

    MockDecoderObject decoderObject;
    KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
    ResourceLoadStatistics statistics{};
    statistics.isVeryPrevalentResource = true;
    statistics.gotLinkDecorationFromPrevalentResource = true;
    statistics.timesAccessedAsFirstPartyDueToUserInteraction = 11;
    statistics.timesAccessedAsFirstPartyDueToStorageAccessAPI = 12;
    bool result = _ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 10);
    Require(result);
    Require(ImplText(statistics.registrableDomain.string.impl) == "ancient.com");
    Require(SetHasDomain(statistics.storageAccessUnderTopFrameDomains, "y.com"));
    Require(SetIsEmpty(statistics.topFrameUniqueRedirectsTo));
    Require(SetIsEmpty(statistics.topFrameUniqueRedirectsFrom));
    Require(statistics.gotLinkDecorationFromPrevalentResource);
    Require(SetIsEmpty(statistics.topFrameLoadedThirdPartyScripts));
    Require(SetIsEmpty(statistics.subframeUnderTopFrameDomains));
    Require(SetHasDomain(statistics.subresourceUnderTopFrameDomains, "z.com"));
    Require(SetHasDomain(statistics.subresourceUniqueRedirectsTo, "w.com"));
    Require(SetIsEmpty(statistics.subresourceUniqueRedirectsFrom));
    Require(!statistics.isPrevalentResource);
    Require(!statistics.isVeryPrevalentResource);
    Require(statistics.dataRecordsRemoved == 8);
    Require(statistics.timesAccessedAsFirstPartyDueToUserInteraction == 11);
    Require(statistics.timesAccessedAsFirstPartyDueToStorageAccessAPI == 12);
}

static void TestDecodeFailures() {
    {
        MockDict root;
        Put(root, "PrevalentResourceDomain", "example.com");
        MockDecoderObject decoderObject;
        KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
        ResourceLoadStatistics statistics{};
        Require(!_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 15));
        Require(ImplText(statistics.registrableDomain.string.impl) == "example.com");
        Require(!statistics.hadUserInteraction);
    }
    {
        MockDict root;
        Put(root, "PrevalentResourceDomain", "example.com");
        Put(root, "hadUserInteraction", true);
        Put(root, "isPrevalentResource", true);
        Put(root, "isVeryPrevalentResource", false);
        Put(root, "dataRecordsRemoved", std::uint32_t{1});
        Put(root, "mostRecentUserInteraction", 1.0);
        Put(root, "grandfathered", false);
        MockDecoderObject decoderObject;
        KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
        ResourceLoadStatistics statistics{};
        Require(!_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 15));
        Require(statistics.isPrevalentResource);
        Require(statistics.dataRecordsRemoved == 1);
        Require(statistics.mostRecentUserInteractionTime.m_value == 1.0);
        Require(!statistics.grandfathered);
        Require(statistics.lastSeen.m_value == 0.0);
    }
    {
        MockDict root;
        Put(root, "PrevalentResourceDomain", "example.com");
        Put(root, "hadUserInteraction", true);
        Put(root, "isPrevalentResource", 1.5);
        MockDecoderObject decoderObject;
        KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
        ResourceLoadStatistics statistics{};
        Require(!_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 15));
    }
    {
        MockDict root;
        Put(root, "PrevalentResourceDomain", "example.com");
        Put(root, "hadUserInteraction", true);
        {
            auto& missing = AddArrayElement(root, "storageAccessUnderTopFrameDomains");
            Put(missing, "other", "x");
        }
        Put(root, "isPrevalentResource", true);
        Put(root, "isVeryPrevalentResource", false);
        Put(root, "dataRecordsRemoved", std::uint32_t{1});
        Put(root, "mostRecentUserInteraction", 1.0);
        Put(root, "grandfathered", false);
        Put(root, "lastSeen", 1.0);
        MockDecoderObject decoderObject;
        KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
        ResourceLoadStatistics statistics{};
        Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 15));
        Require(SetIsEmpty(statistics.storageAccessUnderTopFrameDomains));
    }
    {
        MockDict root;
        Put(root, "PrevalentResourceDomain", "");
        Put(root, "hadUserInteraction", true);
        Put(root, "isPrevalentResource", true);
        Put(root, "isVeryPrevalentResource", false);
        Put(root, "dataRecordsRemoved", std::uint32_t{1});
        Put(root, "mostRecentUserInteraction", 1.0);
        Put(root, "grandfathered", false);
        Put(root, "lastSeen", 1.0);
        MockDecoderObject decoderObject;
        KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
        ResourceLoadStatistics statistics{};
        Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 15));
        Require(ImplText(statistics.registrableDomain.string.impl) == "nullOrigin");
    }
}

static void TestDecodeHashTableGrowth() {
    for (unsigned count : {5u, 6u, 7u, 12u}) {
        MockDict root;
        Put(root, "PrevalentResourceDomain", "example.com");
        Put(root, "hadUserInteraction", true);
        for (unsigned index = 0; index < count; ++index) {
            std::string domain = "d" + std::to_string(index) + ".com";
            auto& element = AddArrayElement(root, "subresourceUniqueRedirectsTo");
            Put(element, "domain", domain.c_str());
        }
        Put(root, "isPrevalentResource", true);
        Put(root, "isVeryPrevalentResource", false);
        Put(root, "dataRecordsRemoved", std::uint32_t{1});
        Put(root, "mostRecentUserInteraction", 1.0);
        Put(root, "grandfathered", false);
        Put(root, "lastSeen", 1.0);
        MockDecoderObject decoderObject;
        KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
        ResourceLoadStatistics statistics{};
        Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 15));
        Require(SetContents(statistics.subresourceUniqueRedirectsTo).size() == count);
        unsigned expectedTableSize = count <= 5 ? 8 : (count <= 11 ? 16 : 32);
        Require(SetTableSize(statistics.subresourceUniqueRedirectsTo) == expectedTableSize);
        for (unsigned index = 0; index < count; ++index) {
            std::string domain = "d" + std::to_string(index) + ".com";
            Require(SetHasDomain(statistics.subresourceUniqueRedirectsTo, domain.c_str()));
        }
    }
}

static void TestMergeSets() {
    MockDict fromRoot = VersionFifteenPayload();
    MockDecoderObject fromObject;
    KeyedDecoder* fromDecoder = MakeDecoder(fromObject, fromRoot);
    ResourceLoadStatistics from{};
    Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&from, fromDecoder, 17));

    ResourceLoadStatistics into{};
    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&into, &from);
    Require(SetHasDomain(into.storageAccessUnderTopFrameDomains, "a.com"));
    Require(SetHasDomain(into.topFrameUniqueRedirectsFrom, "b.com"));
    Require(SetHasDomain(into.topFrameLoadedThirdPartyScripts, "c.com"));
    Require(SetHasDomain(into.subframeUnderTopFrameDomains, "d.com"));
    Require(SetHasDomain(into.subresourceUnderTopFrameDomains, "e.com"));
    Require(SetContents(into.storageAccessUnderTopFrameDomains).size() == 1);

    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&into, &from);
    Require(SetContents(into.storageAccessUnderTopFrameDomains).size() == 1);
    Require(SetTableSize(into.storageAccessUnderTopFrameDomains) == 8);
    const StringImpl* merged = FirstDomainImpl(into.storageAccessUnderTopFrameDomains);
    Require(merged);
    Require(merged->refCount == 4);

    ResourceLoadStatistics caseOther{};
    MockDict caseRoot;
    Put(caseRoot, "PrevalentResourceDomain", "example.com");
    Put(caseRoot, "hadUserInteraction", true);
    {
        auto& element = AddArrayElement(caseRoot, "storageAccessUnderTopFrameDomains");
        Put(element, "domain", "A.COM");
    }
    {
        auto& element = AddArrayElement(caseRoot, "storageAccessUnderTopFrameDomains");
        Put(element, "domain", "a.com");
    }
    Put(caseRoot, "isPrevalentResource", true);
    Put(caseRoot, "isVeryPrevalentResource", false);
    Put(caseRoot, "dataRecordsRemoved", std::uint32_t{1});
    Put(caseRoot, "mostRecentUserInteraction", 1.0);
    Put(caseRoot, "grandfathered", false);
    Put(caseRoot, "lastSeen", 1.0);
    MockDecoderObject caseObject;
    KeyedDecoder* caseDecoder = MakeDecoder(caseObject, caseRoot);
    Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&caseOther, caseDecoder, 15));
    Require(SetContents(caseOther.storageAccessUnderTopFrameDomains).size() == 1);
    _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(&into, &caseOther);
    Require(SetContents(into.storageAccessUnderTopFrameDomains).size() == 1);
    Require(ImplText(FirstDomainImpl(into.storageAccessUnderTopFrameDomains)) == "a.com");
}

static void TestToString() {
    MockDict root = VersionFifteenPayload();
    MockDecoderObject decoderObject;
    KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
    ResourceLoadStatistics statistics{};
    Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 17));

    String text = _ZNK7WebCore22ResourceLoadStatistics8toStringEv(&statistics);
    std::string expected = MergeText({
        "Registrable domain: example.com\n",
        "    hadUserInteraction: Yes\n",
        "    mostRecentUserInteraction: -1\n",
        "    grandfathered: No\n",
        "    storageAccessUnderTopFrameDomains:\n",
        "        a.com\n",
        "    topFrameUniqueRedirectsFrom:\n",
        "        b.com\n",
        "    gotLinkDecorationFromPrevalentResource: Yes\n",
        "    topFrameLoadedThirdPartyScripts:\n",
        "        c.com\n",
        "    subframeUnderTopFrameDomains:\n",
        "        d.com\n",
        "    subresourceUnderTopFrameDomains:\n",
        "        e.com\n",
        "    isPrevalentResource: Yes\n",
        "    isVeryPrevalentResource: No\n",
        "    dataRecordsRemoved: 42\n",
    });
    Require(ImplText(text.impl) == expected);
    Require(text.impl->refCount == 2);
    Require(statistics.registrableDomain.string.impl->refCount == 2);
    Require(SetContents(statistics.storageAccessUnderTopFrameDomains).size() == 1);

    ResourceLoadStatistics empty{};
    String emptyText = _ZNK7WebCore22ResourceLoadStatistics8toStringEv(&empty);
    std::string expectedEmpty = MergeText({
        "Registrable domain: \n",
        "    hadUserInteraction: No\n",
        "    mostRecentUserInteraction: -1\n",
        "    grandfathered: No\n",
        "    gotLinkDecorationFromPrevalentResource: No\n",
        "    isPrevalentResource: No\n",
        "    isVeryPrevalentResource: No\n",
        "    dataRecordsRemoved: 0\n",
    });
    Require(ImplText(emptyText.impl) == expectedEmpty);

    ResourceLoadStatistics recent{};
    recent.mostRecentUserInteractionTime = _ZN3WTF8WallTime3nowEv();
    recent.hadUserInteraction = true;
    String recentText = _ZNK7WebCore22ResourceLoadStatistics8toStringEv(&recent);
    Require(ImplText(recentText.impl).find("    mostRecentUserInteraction: within 24 hours\n") != std::string::npos);
}

static void TestEncode() {
    MockDict root = VersionFifteenPayload();
    MockDecoderObject decoderObject;
    KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
    ResourceLoadStatistics statistics{};
    Require(_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 17));

    std::vector<RecordedCall> calls;
    RecordedCalls = &calls;
    KeyedEncoder encoder = MakeEncoder();
    _ZNK7WebCore22ResourceLoadStatistics6encodeERNS_12KeyedEncoderE(&statistics, &encoder);
    RecordedCalls = nullptr;

    Require(calls.size() == 36);
    Require(calls[0].kind == RecordedCall::Kind::String);
    Require(calls[0].key == "PrevalentResourceDomain");
    Require(calls[0].stringValue == "example.com");
    Require(calls[1].kind == RecordedCall::Kind::Double);
    Require(calls[1].key == "lastSeen");
    Require(calls[1].doubleValue == 1000.5);
    Require(calls[2].kind == RecordedCall::Kind::Bool);
    Require(calls[2].key == "hadUserInteraction");
    Require(calls[2].boolValue);
    Require(calls[3].kind == RecordedCall::Kind::Double);
    Require(calls[3].key == "mostRecentUserInteraction");
    Require(calls[3].doubleValue == 0.0);
    Require(calls[4].kind == RecordedCall::Kind::Bool);
    Require(calls[4].key == "grandfathered");
    Require(!calls[4].boolValue);

    Require(calls[5].kind == RecordedCall::Kind::BeginArray);
    Require(calls[5].key == "storageAccessUnderTopFrameDomains");
    Require(calls[6].kind == RecordedCall::Kind::BeginArrayElement);
    Require(calls[7].kind == RecordedCall::Kind::String);
    Require(calls[7].key == "domain");
    Require(calls[7].stringValue == "a.com");
    Require(calls[8].kind == RecordedCall::Kind::EndArrayElement);
    Require(calls[9].kind == RecordedCall::Kind::EndArray);

    Require(calls[10].kind == RecordedCall::Kind::BeginArray);
    Require(calls[10].key == "topFrameUniqueRedirectsFrom");
    Require(calls[12].stringValue == "b.com");
    Require(calls[14].kind == RecordedCall::Kind::EndArray);

    Require(calls[15].kind == RecordedCall::Kind::Bool);
    Require(calls[15].key == "gotLinkDecorationFromPrevalentResource");
    Require(calls[16].kind == RecordedCall::Kind::BeginArray);
    Require(calls[16].key == "topFrameLoadedThirdPartyScripts");
    Require(calls[18].stringValue == "c.com");
    Require(calls[20].kind == RecordedCall::Kind::EndArray);
    Require(calls[21].kind == RecordedCall::Kind::BeginArray);
    Require(calls[21].key == "subframeUnderTopFrameDomains");
    Require(calls[23].stringValue == "d.com");
    Require(calls[25].kind == RecordedCall::Kind::EndArray);
    Require(calls[26].kind == RecordedCall::Kind::BeginArray);
    Require(calls[26].key == "subresourceUnderTopFrameDomains");
    Require(calls[28].stringValue == "e.com");
    Require(calls[30].kind == RecordedCall::Kind::EndArray);
    Require(calls[31].kind == RecordedCall::Kind::Bool);
    Require(calls[31].key == "isPrevalentResource");
    Require(calls[32].kind == RecordedCall::Kind::Bool);
    Require(calls[32].key == "isVeryPrevalentResource");
    Require(calls[33].kind == RecordedCall::Kind::UInt32);
    Require(calls[33].key == "dataRecordsRemoved");
    Require(calls[33].uint32Value == 42);
    Require(calls[34].kind == RecordedCall::Kind::UInt32);
    Require(calls[34].key == "timesAccessedAsFirstPartyDueToUserInteraction");
    Require(calls[34].uint32Value == 7);
    Require(calls[35].kind == RecordedCall::Kind::UInt32);
    Require(calls[35].key == "timesAccessedAsFirstPartyDueToStorageAccessAPI");
    Require(calls[35].uint32Value == 3);

    std::vector<RecordedCall> scalarCalls;
    RecordedCalls = &scalarCalls;
    ResourceLoadStatistics plain{};
    plain.dataRecordsRemoved = 7;
    plain.timesAccessedAsFirstPartyDueToUserInteraction = 8;
    plain.timesAccessedAsFirstPartyDueToStorageAccessAPI = 9;
    KeyedEncoder plainEncoder = MakeEncoder();
    _ZNK7WebCore22ResourceLoadStatistics6encodeERNS_12KeyedEncoderE(&plain, &plainEncoder);
    RecordedCalls = nullptr;
    Require(scalarCalls.size() == 11);
    Require(scalarCalls[10].kind == RecordedCall::Kind::UInt32);
    Require(scalarCalls[10].key == "timesAccessedAsFirstPartyDueToStorageAccessAPI");
    Require(scalarCalls[10].uint32Value == 9);
}

static void TestInvalidArguments() {
    ResourceLoadStatistics statistics{};
    Require(ThrowsInvalidArgument([](ResourceLoadStatistics* self, const ResourceLoadStatistics* other) {
        _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(self, other);
    }, nullptr, &statistics));
    Require(ThrowsInvalidArgument([](ResourceLoadStatistics* self, const ResourceLoadStatistics* other) {
        _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(self, other);
    }, &statistics, nullptr));
    Require(ThrowsInvalidArgument([](ResourceLoadStatistics* self, KeyedDecoder* decoder) {
        _ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(self, decoder, 15);
    }, nullptr, nullptr));
    Require(ThrowsInvalidArgument([](const ResourceLoadStatistics* self, KeyedEncoder* encoder) {
        _ZNK7WebCore22ResourceLoadStatistics6encodeERNS_12KeyedEncoderE(self, encoder);
    }, &statistics, nullptr));
    KeyedEncoder noVtable{};
    Require(ThrowsInvalidArgument([](const ResourceLoadStatistics* self, KeyedEncoder* encoder) {
        _ZNK7WebCore22ResourceLoadStatistics6encodeERNS_12KeyedEncoderE(self, encoder);
    }, &statistics, &noVtable));
    Require(ThrowsInvalidArgument([](const ResourceLoadStatistics* self) {
        _ZNK7WebCore22ResourceLoadStatistics8toStringEv(self);
    }, nullptr));
    MockDict root;
    MockDecoderObject decoderObject;
    KeyedDecoder* decoder = MakeDecoder(decoderObject, root);
    Require(ThrowsInvalidArgument([](ResourceLoadStatistics* self, KeyedDecoder* dec) {
        _ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(self, dec, 15);
    }, &statistics, nullptr));
    Require(!_ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(&statistics, decoder, 15));
}

int main() {
    TestMergeScalars();
    TestDecodeVersionSeventeen();
    TestDecodeVersionFourteen();
    TestDecodeVersionThirteen();
    TestDecodeVersionTen();
    TestDecodeFailures();
    TestDecodeHashTableGrowth();
    TestMergeSets();
    TestToString();
    TestEncode();
    TestInvalidArguments();
    return 0;
}
