#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using NKWebKit::InspectorAbi::BackendDispatcher;
using NKWebKit::InspectorAbi::CallbackBase;
using NKWebKit::InspectorAbi::CommonErrorCode;
using NKWebKit::InspectorAbi::FrontendChannel;
using NKWebKit::InspectorAbi::FrontendRouter;
using NKWebKit::InspectorAbi::FrontendRouterRef;
using NKWebKit::InspectorAbi::SupplementalBackendDispatcher;
using NKWebKit::InspectorAbi::SupplementalBackendDispatcherVtable;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::Value;
using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Inspector backend dispatcher check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename Exception, typename Action> bool Throws(Action action) {
    try {
        action();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

int guestAllocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations++;
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) guestAllocations--;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) guestAllocations++;
    return pointer;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    return std::realloc(pointer, size);
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) {
    return Allocate_nid_no_patch(size);
}

void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) {
    return Reallocate_nid_no_patch(pointer, size);
}

int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

void RegisterHeap() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
}

namespace DispatchDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
    void MixText(const std::u16string& text) {
        Mix(text.size());
        for (char16_t c : text) Mix(c);
    }
};

struct Random {
    std::uint32_t state;
    std::uint32_t operator()(std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state % bound;
    }
};

inline std::u16string Ascii(const char* text) {
    std::u16string result;
    while (*text) result.push_back(static_cast<unsigned char>(*text++));
    return result;
}

inline std::u16string Decimal(long long value) {
    return Ascii(std::to_string(value).c_str());
}

inline const char16_t* const Methods[] = {
    u"Alpha.x", u"Alpha.e0", u"Alpha.e1", u"Alpha.e2", u"Alpha.e3", u"Alpha.e4", u"Alpha.e5", u"Alpha.send", u"Alpha.fail",
    u"Alpha.nest", u"Alpha.twice", u"Beta.e2", u"Beta.y", u"Gamma.x", u"Unknown.y", u"Alpha", u"Alpha.", u".x", u"Alpha.b.c",
    u"", u"Alp\u00e9a.x", u"\u4e2d.x", u"Beta.\u00ff", u"alpha.x", u"Alpha..x",
};

inline std::u16string Space(Random& random) {
    static const char16_t spaces[] = {u' ', u'\t', u'\n', u'\r', u'\v', u'\f', 0x2003, 0x3000, 0x00A0, 0x0085, 0x2028, 0x2029, 0x1680, 0x200B};
    std::u16string result;
    int count = random(4) ? 0 : static_cast<int>(random(3));
    for (int i = 0; i < count; ++i) result.push_back(random(4) ? u' ' : spaces[random(sizeof(spaces) / sizeof(spaces[0]))]);
    return result;
}

inline std::u16string NumberText(Random& random) {
    switch (random(16)) {
    case 0: return u"0";
    case 1: return u"-0";
    case 2: return Decimal(static_cast<long long>(random(2000)) - 1000);
    case 3: return Decimal(static_cast<long long>(random(1u << 31)));
    case 4: return u"2147483648";
    case 5: return u"-2147483649";
    case 6: return u"1e10";
    case 7: return u"3.75";
    case 8: return u"-0.5";
    case 9: return u"1.5e3";
    case 10: return u"1E+2";
    case 11: return u"1e400";
    case 12: return u"1e-400";
    case 13: return u"123456789012345";
    case 14: {
        std::u16string result = Decimal(static_cast<long long>(random(100000)));
        result += u".";
        result += Decimal(static_cast<long long>(random(1000000)));
        result += u"e-";
        result += Decimal(random(30));
        return result;
    }
    default: {
        static const char16_t* bad[] = {u"01", u".5", u"1.", u"-", u"1e", u"+1", u"0x10", u"1e+", u"--1"};
        return bad[random(9)];
    }
    }
}

inline std::u16string StringText(Random& random) {
    std::u16string result = u"\"";
    int length = static_cast<int>(random(8));
    for (int i = 0; i < length; ++i) {
        switch (random(12)) {
        case 0: result += u"\\n"; break;
        case 1: result += u"\\\""; break;
        case 2: result += u"\\\\"; break;
        case 3: result += u"\\u00e9"; break;
        case 4: result += u"\\ud83d\\ude00"; break;
        case 5: result += u"\\x41"; break;
        case 6: result += u"\\v"; break;
        case 7: result.push_back(static_cast<char16_t>(random(2) ? 0x4e2d : 0xe9)); break;
        case 8: result.push_back(static_cast<char16_t>(random(32))); break;
        case 9: result += random(2) ? u"<" : u">"; break;
        case 10: result += random(8) ? u"\\/" : u"\\q"; break;
        default: result.push_back(static_cast<char16_t>(u'a' + random(26))); break;
        }
    }
    result += u"\"";
    return result;
}

inline std::u16string ValueText(Random& random, int depth) {
    switch (depth > 3 ? random(5) : random(8)) {
    case 0: return u"null";
    case 1: return random(2) ? u"true" : u"false";
    case 2: return NumberText(random);
    case 3: return StringText(random);
    case 4: return random(16) ? u"1" : u"nul";
    case 5: {
        std::u16string result = u"[";
        result += Space(random);
        int count = static_cast<int>(random(4));
        for (int i = 0; i < count; ++i) {
            if (i) result += u",";
            result += ValueText(random, depth + 1);
            result += Space(random);
        }
        if (!random(20)) result += u",";
        return result + u"]";
    }
    default: {
        std::u16string result = u"{";
        int count = static_cast<int>(random(5));
        for (int i = 0; i < count; ++i) {
            if (i) result += u",";
            result += Space(random);
            if (random(10)) {
                result += u"\"k";
                result += Decimal(random(6));
                result += u"\"";
            } else {
                result += StringText(random);
            }
            result += Space(random);
            result += u":";
            result += ValueText(random, depth + 1);
        }
        if (!random(20)) result += u",";
        return result + u"}";
    }
    }
}

inline std::u16string Nested(int depth) {
    std::u16string result = u"{\"id\":7,\"method\":\"Alpha.x\",\"params\":";
    for (int i = 0; i < depth; ++i) result += u"[";
    for (int i = 0; i < depth; ++i) result += u"]";
    return result + u"}";
}

inline std::u16string Message(Random& random) {
    std::u16string id;
    switch (random(10)) {
    case 0: break;
    case 1: id = StringText(random); break;
    case 2: id = random(2) ? u"true" : u"null"; break;
    case 3: id = u"{}"; break;
    default: id = NumberText(random); break;
    }
    std::u16string method;
    switch (random(10)) {
    case 0: break;
    case 1: method = NumberText(random); break;
    case 2: method = u"null"; break;
    default: method = u"\"" + std::u16string(Methods[random(sizeof(Methods) / sizeof(Methods[0]))]) + u"\""; break;
    }
    std::u16string members;
    auto add = [&](const std::u16string& member) {
        if (!members.empty()) members += u",";
        members += Space(random);
        members += member;
        members += Space(random);
    };
    if (!random(4)) add(u"\"params\":" + ValueText(random, 0));
    if (!id.empty()) add(u"\"id\":" + id);
    if (!random(6)) add(u"\"extra\":" + ValueText(random, 1));
    if (!method.empty()) add(u"\"method\":" + method);
    if (!random(12)) add(u"\"id\":" + NumberText(random));
    if (!random(12)) add(u"\"method\":\"Alpha.e3\"");
    std::u16string text = Space(random);
    text += u"{";
    text += members;
    text += u"}";
    text += Space(random);
    switch (random(24)) {
    case 0: return text.substr(0, random(static_cast<std::uint32_t>(text.size()) + 1));
    case 1: return ValueText(random, 0);
    case 2: return text + u"x";
    case 3: return Nested(997 + static_cast<int>(random(6)));
    case 4: return u"";
    default: return text;
    }
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count) {
    Hash hash;
    Random random{seed ? seed : 1};
    adapter.Reset();
    for (int i = 0; i < count; ++i) {
        if (!random(16)) adapter.SetConnected(random(3) != 0);
        std::u16string message = Message(random);
        hash.MixText(message);
        adapter.Dispatch(message, random(2));
        for (const auto& line : adapter.TakeTranscript()) hash.MixText(line);
        hash.Mix(adapter.HasProtocolErrors());
    }
    return hash.value;
}

}  // namespace DispatchDriver

std::u16string ToU16(const String& string) {
    std::u16string result;
    if (!string.impl) return result;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned i = 0; i < string.impl->length; ++i) result.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i));
    return result;
}

String FromU16(const std::u16string& text, bool as8Bit) {
    bool fits = true;
    for (char16_t c : text) fits = fits && c <= 0xFF;
    const bool is8Bit = as8Bit && fits;
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.size()), is8Bit, characters));
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(text[i]);
        else static_cast<char16_t*>(characters)[i] = text[i];
    }
    return result;
}

String Text(const char* text) {
    return FromU16(DispatchDriver::Ascii(text), true);
}

FrontendRouter* TakeRouter() {
    FrontendRouterRef router = _ZN9Inspector14FrontendRouter6createEv();
    return std::exchange(router.ptr, nullptr);
}

std::vector<std::u16string>* activeTranscript;

void APS5_VABI ChannelDestructor(FrontendChannel*) {
}

NKWebKit::InspectorAbi::ConnectionType APS5_VABI ChannelConnectionType(const FrontendChannel*) {
    return NKWebKit::InspectorAbi::ConnectionType::Remote;
}

void APS5_VABI ChannelSendMessage(FrontendChannel*, const String* message) {
    activeTranscript->push_back(u"S:" + ToU16(*message));
}

void* const ChannelVtable[] = {reinterpret_cast<void*>(&ChannelDestructor), reinterpret_cast<void*>(&ChannelDestructor),
    reinterpret_cast<void*>(&ChannelConnectionType), reinterpret_cast<void*>(&ChannelSendMessage)};

struct Adapter;

struct ScriptedDispatcher {
    SupplementalBackendDispatcher base;
    Adapter* adapter;
    const char16_t* name;
};

void APS5_VABI ScriptedDestructor(SupplementalBackendDispatcher* self) {
    _ZN9Inspector29SupplementalBackendDispatcherD2Ev(self);
}

void APS5_VABI ScriptedDispatch(SupplementalBackendDispatcher* self, std::int64_t requestId, const String* method, Object** message);

const SupplementalBackendDispatcherVtable ScriptedVtable{ScriptedDestructor, ScriptedDestructor, ScriptedDispatch};

struct Adapter {
    std::vector<std::u16string> transcript;
    FrontendChannel channel{ChannelVtable};
    FrontendRouter* router = nullptr;
    BackendDispatcher* backend = nullptr;
    ScriptedDispatcher alpha{};
    ScriptedDispatcher beta{};
    bool connected = false;
    Adapter() { activeTranscript = &transcript; }
    ~Adapter() { Release(); }
    void Release() {
        if (!backend) return;
        _ZN9Inspector29SupplementalBackendDispatcherD2Ev(&alpha.base);
        _ZN9Inspector29SupplementalBackendDispatcherD2Ev(&beta.base);
        NKWebKit::InspectorAbi::DerefBackendDispatcher(std::exchange(backend, nullptr));
        if (connected) _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router, &channel);
        FrontendRouterRef(std::exchange(router, nullptr));
    }
    void Init(ScriptedDispatcher& dispatcher, const char16_t* name) {
        _ZN9Inspector29SupplementalBackendDispatcherC2ERNS_17BackendDispatcherE(&dispatcher.base, backend);
        dispatcher.base.vptr = &ScriptedVtable;
        dispatcher.adapter = this;
        dispatcher.name = name;
    }
    void Reset() {
        Release();
        transcript.clear();
        router = TakeRouter();
        connected = false;
        SetConnected(true);
        backend = NKWebKit::InspectorAbi::CreateBackendDispatcher(*router);
        NKWebKit::InspectorAbi::DerefBackendDispatcher(nullptr);
        Init(alpha, u"Alpha");
        Init(beta, u"Beta");
        String alphaName = Text("Alpha");
        String betaName = Text("Beta");
        String gammaName = Text("Gamma");
        _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(backend, &alphaName, &alpha.base);
        _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(backend, &gammaName, &alpha.base);
        _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(backend, &betaName, &beta.base);
        _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(backend, &gammaName, &beta.base);
    }
    void SetConnected(bool value) {
        if (value == connected) return;
        if (value) _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router, &channel);
        else _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router, &channel);
        connected = value;
    }
    void Dispatch(const std::u16string& message, bool as8Bit) {
        String text = FromU16(message, as8Bit);
        _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(backend, &text);
    }
    std::vector<std::u16string> TakeTranscript() { return std::exchange(transcript, {}); }
    bool HasProtocolErrors() { return _ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(backend); }
};

void Report(BackendDispatcher* backend, CommonErrorCode code, const std::u16string& message) {
    String text = FromU16(message, false);
    _ZN9Inspector17BackendDispatcher19reportProtocolErrorENS0_15CommonErrorCodeERKN3WTF6StringE(backend, code, &text);
}

void APS5_VABI ScriptedDispatch(SupplementalBackendDispatcher* self, std::int64_t requestId, const String* method, Object** message) {
    auto& scripted = *reinterpret_cast<ScriptedDispatcher*>(self);
    BackendDispatcher* backend = self->backendDispatcher;
    std::u16string name = ToU16(*method);
    String json = _ZNK3WTF8JSONImpl5Value12toJSONStringEv(*message);
    scripted.adapter->transcript.push_back(u"D:" + std::u16string(scripted.name) + u":" + DispatchDriver::Decimal(requestId) + u":" + name + u":" + ToU16(json) + u":"
        + (_ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(backend) ? u"1" : u"0") + (_ZNK9Inspector17BackendDispatcher8isActiveEv(backend) ? u"1" : u"0"));
    if (name == u"send") {
        _ZN9Inspector17BackendDispatcher17sendPendingErrorsEv(backend);
    } else if (name.size() == 2 && name[0] == u'e' && name[1] >= u'0' && name[1] <= u'5') {
        Report(backend, static_cast<CommonErrorCode>(name[1] - u'0'), u"failure " + name);
    } else if (name == u"fail") {
        NKWebKit::InspectorAbi::RefBackendDispatcher(*backend);
        CallbackBase callback{1, {}, backend, requestId, false, {}};
        scripted.adapter->transcript.push_back(_ZNK9Inspector17BackendDispatcher12CallbackBase8isActiveEv(&callback) ? u"C:1" : u"C:0");
        String failed = Text("callback failed");
        String again = Text("again");
        _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(&callback, &failed);
        _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(&callback, &again);
        scripted.adapter->transcript.push_back(_ZNK9Inspector17BackendDispatcher12CallbackBase8isActiveEv(&callback) ? u"C:1" : u"C:0");
        _ZN9Inspector17BackendDispatcher12CallbackBaseD1Ev(&callback);
    } else if (name == u"nest") {
        String nested = Text("{\"id\":424242,\"method\":\"Alpha.e1\"}");
        _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(backend, &nested);
        Report(backend, CommonErrorCode::InternalError, u"after nest");
    } else if (name == u"twice") {
        Report(backend, CommonErrorCode::InvalidParams, u"first");
        Report(backend, CommonErrorCode::InternalError, u"second");
    }
}

const std::uint64_t ExpectedHashes[] = {
    0xcb571a56b68d96c4ull,
    0x9368ede3f6e1d9adull,
    0x44cbc859963aeb8bull,
    0x176ea4865a097c54ull,
    0xa573c89897ba297eull,
    0x47df07774f088a03ull,
    0x26a30b2dfb654865ull,
    0x6aad6fb0691cbf15ull,
    0x45692b14d7379112ull,
    0x7d6b38ba771c33f3ull,
    0x2260e750c80f9e6full,
    0x67aef6be1e45a7f2ull,
    0xe942be921b505ff8ull,
    0x1ba34b624b94b718ull,
    0x075e5f2c2d23e694ull,
    0xaad37db8b86329b5ull,
};

void TestOracleTranscripts() {
    for (std::size_t seed = 0; seed < std::size(ExpectedHashes); ++seed) {
        Adapter adapter;
        Require(DispatchDriver::Run(adapter, static_cast<std::uint32_t>(seed + 1), 400) == ExpectedHashes[seed]);
    }
}

std::u16string Json(NKWebKit::JsonAbi::Ref<Value>&& value) {
    String text = _ZNK3WTF8JSONImpl5Value12toJSONStringEv(value.Get());
    return ToU16(text);
}

void TestWriter() {
    Require(Json(_ZN3WTF8JSONImpl5Value6createERKNS_6StringE(FromU16(u"<a>\u00e9\n\"\\\u2028\x01/", false))) == u"\"\\u003Ca\\u003E\\u00E9\\n\\\"\\\\\\u2028\\u0001/\"");
    Require(Json(_ZN3WTF8JSONImpl5Value6createERKNS_6StringE(String())) == u"\"\"");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEd(0.1)) == u"0.1");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEd(1e21)) == u"1e+21");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEd(-0.0)) == u"0");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEd(std::nan(""))) == u"null");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEd(-std::numeric_limits<double>::infinity())) == u"null");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEi(-2147483647 - 1)) == u"-2147483648");
    Require(Json(_ZN3WTF8JSONImpl5Value6createEb(false)) == u"false");
    Require(Json(_ZN3WTF8JSONImpl5Value4nullEv()) == u"null");

    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    for (int i = 0; i < 40; ++i) NKWebKit::JsonAbi::SetValue(*object, Text(("k" + std::to_string(i % 25)).c_str()), _ZN3WTF8JSONImpl5Value6createEi(i));
    std::u16string expected = u"{";
    for (int i = 0; i < 25; ++i) expected += (i ? u"," : u"") + DispatchDriver::Ascii(("\"k" + std::to_string(i) + "\":" + std::to_string(i < 15 ? i + 25 : i)).c_str());
    Require(Json(NKWebKit::JsonAbi::Ref<Value>::Adopt(object.Leak())) == expected + u"}");

    auto array = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(array.Get(), 1);
    _ZN3WTF8JSONImpl9ArrayBase10pushStringERKNS_6StringE(array.Get(), Text("x"));
    NKWebKit::JsonAbi::PushValue(*array, NKWebKit::JsonAbi::Ref<Value>::Adopt(_ZN3WTF8JSONImpl5Array6createEv().Leak()));
    NKWebKit::WtfAbi::StringBuilder builder{};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    NKWebKit::WtfAbi::AppendCString(builder, "=");
    _ZNK3WTF8JSONImpl9ArrayBase9writeJSONERNS_13StringBuilderE(array.Get(), &builder);
    _ZNK3WTF8JSONImpl5Value9writeJSONERNS_13StringBuilderE(_ZN3WTF8JSONImpl5Value6createEb(true).Get(), &builder);
    Require(ToU16(NKWebKit::WtfAbi::ToString(builder)) == u"=[1,\"x\",[]]true");
    NKWebKit::WtfAbi::Clear(builder);
    Require(Throws<std::invalid_argument>([&] { _ZNK3WTF8JSONImpl9ArrayBase9writeJSONERNS_13StringBuilderE(array.Get(), nullptr); }));
}

void TestParserLimits() {
    Require(!NKWebKit::JsonAbi::ParseJSON(String()));
    Require(static_cast<bool>(NKWebKit::JsonAbi::ParseJSON(FromU16(u"\u3000[1]\u2003\n", false))));
    Require(!NKWebKit::JsonAbi::ParseJSON(FromU16(u"[1]\u00a0", false)));
    Require(!NKWebKit::JsonAbi::ParseJSON(FromU16(u"[1]\u200b", false)));
    auto nested = [](int depth) {
        std::u16string text(depth, u'[');
        text += std::u16string(depth, u']');
        return NKWebKit::JsonAbi::ParseJSON(FromU16(text, true));
    };
    Require(static_cast<bool>(nested(1001)));
    Require(!nested(1002));
    auto value = NKWebKit::JsonAbi::ParseJSON(Text("{\"a\":\"\\x41\\v\\u00e9\",\"a\":1e400,\"b\":-0}"));
    Require(value && ToU16(_ZNK3WTF8JSONImpl5Value12toJSONStringEv(value.Get())) == u"{\"a\":null,\"b\":0}");
}

void TestLifetimes() {
    const int before = guestAllocations;
    FrontendRouter* router = TakeRouter();
    BackendDispatcher* backend = NKWebKit::InspectorAbi::CreateBackendDispatcher(*router);
    Require(router->refCount == 2 && backend->refCount == 1 && !_ZNK9Inspector17BackendDispatcher8isActiveEv(backend));

    auto* supplemental = static_cast<SupplementalBackendDispatcher*>(_Znwm_nid_postfix(sizeof(SupplementalBackendDispatcher)));
    _ZN9Inspector29SupplementalBackendDispatcherC2ERNS_17BackendDispatcherE(supplemental, backend);
    Require(supplemental->refCount == 1 && backend->refCount == 2);
    String domain = Text("Domain");
    _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(backend, &domain, supplemental);
    String message = Text("{\"id\":1,\"method\":\"Domain.x\"}");
    Require(Throws<std::logic_error>([&] { _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(backend, &message); }));
    Require(backend->refCount == 2);
    supplemental->vptr->deletingDestructor(supplemental);
    Require(backend->refCount == 1);

    String error = Text("pending");
    _ZN9Inspector17BackendDispatcher19reportProtocolErrorENS0_15CommonErrorCodeERKN3WTF6StringE(backend, CommonErrorCode::ServerError, &error);
    Require(_ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(backend) && !backend->hasCurrentRequestId);
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher19reportProtocolErrorENS0_15CommonErrorCodeERKN3WTF6StringE(backend, static_cast<CommonErrorCode>(6), &error); }));
    _ZN9Inspector17BackendDispatcher17sendPendingErrorsEv(backend);
    Require(!_ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(backend) && !backend->protocolErrors);

    NKWebKit::InspectorAbi::RefBackendDispatcher(*backend);
    CallbackBase callback{1, {}, backend, 5, false, {}};
    _ZN9Inspector17BackendDispatcher12CallbackBase7disableEv(&callback);
    String ignored = Text("ignored");
    _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(&callback, &ignored);
    Require(callback.alreadySent && !_ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(backend));
    _ZN9Inspector17BackendDispatcher12CallbackBaseD2Ev(&callback);
    Require(backend->refCount == 1 && !callback.backendDispatcher);

    NKWebKit::InspectorAbi::DerefBackendDispatcher(backend);
    Require(router->refCount == 1);
    FrontendRouterRef routerRef(router);
    (void)routerRef;
    Require(guestAllocations == before);
}

void TestLargeRequestIds() {
    Adapter adapter;
    adapter.Reset();
    adapter.Dispatch(u"{\"id\":4294967301,\"method\":\"Alpha.e0\"}", true);
    auto lines = adapter.TakeTranscript();
    Require(lines.size() == 2);
    Require(lines[0].find(u"D:Alpha:4294967301:e0:") == 0);
    Require(lines[1] == u"S:{\"error\":{\"code\":-32700,\"message\":\"failure e0\",\"data\":[{\"code\":-32700,\"message\":\"failure e0\"}]},\"id\":5}");
    adapter.Dispatch(u"{\"id\":1e300,\"method\":\"Nope.x\"}", true);
    lines = adapter.TakeTranscript();
    Require(lines.size() == 1 && lines[0] == u"S:{\"error\":{\"code\":-32601,\"message\":\"'Nope' domain was not found\",\"data\":[{\"code\":-32601,\"message\":\"'Nope' domain was not found\"}]},\"id\":0}");
}

void TestNullArguments() {
    String text = Text("x");
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(nullptr, &text); }));
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher17sendPendingErrorsEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK9Inspector17BackendDispatcher8isActiveEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector29SupplementalBackendDispatcherC2ERNS_17BackendDispatcherE(nullptr, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher12CallbackBase7disableEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK3WTF8JSONImpl5Value12toJSONStringEv(nullptr); }));
    FrontendRouter* router = TakeRouter();
    BackendDispatcher* backend = NKWebKit::InspectorAbi::CreateBackendDispatcher(*router);
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(backend, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(backend, &text, nullptr); }));
    String nullDomain;
    SupplementalBackendDispatcher dummy{};
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(backend, &nullDomain, &dummy); }));
    Require(!backend->dispatchers);
    NKWebKit::InspectorAbi::DerefBackendDispatcher(backend);
    FrontendRouterRef routerRef(router);
    (void)routerRef;
}

}  // namespace

int main() {
    RegisterHeap();
    TestOracleTranscripts();
    TestWriter();
    TestParserLimits();
    TestLifetimes();
    TestLargeRequestIds();
    TestNullArguments();
    std::puts("Inspector backend dispatcher tests passed");
    return 0;
}
