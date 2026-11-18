#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InspectorFrontendEventExports.hpp"
#include "prx/libSceNKWebKit/include/InspectorFrontendRouterExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using NKWebKit::InspectorAbi::FrontendChannel;
using NKWebKit::InspectorAbi::FrontendDispatcher;
using NKWebKit::InspectorAbi::FrontendRouterRef;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "InspectorFrontendEvents check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Exception, typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

namespace FrontendEventDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
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

struct Argument {
    std::u16string text;
    bool isNull = false;
    bool is8Bit = true;
    int integer = 0;
    double number = 0;
    bool boolean = false;
};

struct Message {
    bool is8Bit;
    std::u16string text;
};

struct ParameterKind {
    char kind;
    int enumBase;
    int enumCount;
};

struct EventShape {
    int count;
    ParameterKind parameters[5];
};

inline const EventShape Events[] = {
    {1, {{'S', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {0, {}},
    {0, {}},
    {2, {{'I', 0, 0}, {'S', 0, 0}}},
    {2, {{'I', 0, 0}, {'I', 0, 0}}},
    {2, {{'I', 0, 0}, {'I', 0, 0}}},
    {3, {{'I', 0, 0}, {'S', 0, 0}, {'S', 0, 0}}},
    {1, {{'I', 0, 0}}},
    {2, {{'I', 0, 0}, {'I', 0, 0}}},
    {2, {{'I', 0, 0}, {'S', 0, 0}}},
    {2, {{'I', 0, 0}, {'I', 0, 0}}},
    {1, {{'I', 0, 0}}},
    {2, {{'I', 0, 0}, {'E', 112, 4}}},
    {3, {{'I', 0, 0}, {'D', 0, 0}, {'B', 0, 0}}},
    {1, {{'I', 0, 0}}},
    {2, {{'D', 0, 0}, {'S', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {1, {{'D', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {1, {{'D', 0, 0}}},
    {2, {{'S', 0, 0}, {'D', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {2, {{'S', 0, 0}, {'S', 0, 0}}},
    {2, {{'S', 0, 0}, {'E', 222, 3}}},
    {1, {{'S', 0, 0}}},
    {2, {{'S', 0, 0}, {'D', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {2, {{'S', 0, 0}, {'S', 0, 0}}},
    {2, {{'S', 0, 0}, {'S', 0, 0}}},
    {3, {{'S', 0, 0}, {'S', 0, 0}, {'S', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {2, {{'S', 0, 0}, {'S', 0, 0}}},
    {4, {{'S', 0, 0}, {'D', 0, 0}, {'I', 0, 0}, {'I', 0, 0}}},
    {2, {{'S', 0, 0}, {'D', 0, 0}}},
    {2, {{'S', 0, 0}, {'S', 0, 0}}},
    {3, {{'S', 0, 0}, {'D', 0, 0}, {'S', 0, 0}}},
    {0, {}},
    {5, {{'S', 0, 0}, {'S', 0, 0}, {'I', 0, 0}, {'I', 0, 0}, {'S', 0, 0}}},
    {1, {{'I', 0, 0}}},
    {0, {}},
    {1, {{'D', 0, 0}}},
    {1, {{'D', 0, 0}}},
    {0, {}},
    {1, {{'S', 0, 0}}},
    {1, {{'D', 0, 0}}},
    {1, {{'D', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {1, {{'S', 0, 0}}},
    {0, {}},
    {1, {{'D', 0, 0}}},
    {1, {{'B', 0, 0}}},
    {3, {{'S', 0, 0}, {'S', 0, 0}, {'I', 0, 0}}},
};

constexpr int EventCount = sizeof(Events) / sizeof(Events[0]);

inline Argument MakeString(Random& random) {
    Argument argument;
    unsigned shape = random(10);
    if (!shape) {
        argument.isNull = true;
        return argument;
    }
    bool wide = shape >= 6;
    unsigned length = random(9);
    for (unsigned i = 0; i < length; ++i) {
        char16_t c;
        switch (random(12)) {
        case 0: c = static_cast<char16_t>(random(0x20)); break;
        case 1: c = u'"'; break;
        case 2: c = u'\\'; break;
        case 3: c = u'/'; break;
        case 4: c = static_cast<char16_t>(0x7F + random(0x81)); break;
        case 5: c = wide ? static_cast<char16_t>(0xD800 + random(0x800)) : u'x'; break;
        case 6: c = wide ? static_cast<char16_t>(0x100 + random(0xD700)) : u'y'; break;
        case 7: c = wide ? static_cast<char16_t>(0xE000 + random(0x2000)) : u'z'; break;
        default: c = static_cast<char16_t>(u'a' + random(26)); break;
        }
        argument.text.push_back(c);
        if (wide && c >= 0xD800 && c < 0xDC00 && random(2)) argument.text.push_back(static_cast<char16_t>(0xDC00 + random(0x400)));
    }
    argument.is8Bit = !wide;
    return argument;
}

inline double MakeDouble(Random& random) {
    switch (random(10)) {
    case 0: return 0.0;
    case 1: return -0.0;
    case 2: return std::numeric_limits<double>::quiet_NaN();
    case 3: return random(2) ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
    case 4: return static_cast<double>(static_cast<int>(random(2000000)) - 1000000);
    case 5: return static_cast<double>(random(100000)) / 1000.0;
    case 6: return 1e21 * static_cast<double>(1 + random(9));
    case 7: return 1e-7 * static_cast<double>(1 + random(999));
    default: {
        std::uint64_t bits = random(0x10000);
        bits = (bits << 16) | random(0x10000);
        bits = (bits << 16) | random(0x10000);
        bits = (bits << 16) | random(0x10000);
        bits &= 0x7FEFFFFFFFFFFFFFull;
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        return random(2) ? -value : value;
    }
    }
}

inline int MakeInteger(Random& random) {
    switch (random(6)) {
    case 0: return 0;
    case 1: return -1;
    case 2: return std::numeric_limits<int>::min();
    case 3: return std::numeric_limits<int>::max();
    default: {
        std::uint32_t bits = random(0x10000);
        bits = (bits << 16) | random(0x10000);
        int value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    }
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count) {
    Hash hash;
    Random random{seed ? seed : 1};
    for (int step = 0; step < count; ++step) {
        int index = static_cast<int>(random(EventCount));
        const EventShape& shape = Events[index];
        Argument arguments[5];
        for (int i = 0; i < shape.count; ++i) {
            switch (shape.parameters[i].kind) {
            case 'S': arguments[i] = MakeString(random); break;
            case 'I': arguments[i].integer = MakeInteger(random); break;
            case 'D': arguments[i].number = MakeDouble(random); break;
            case 'B': arguments[i].boolean = random(2); break;
            default: arguments[i].integer = shape.parameters[i].enumBase + static_cast<int>(random(static_cast<std::uint32_t>(shape.parameters[i].enumCount))); break;
            }
        }
        std::vector<Message> messages = adapter.Call(index, arguments);
        hash.Mix(static_cast<std::uint64_t>(index));
        hash.Mix(messages.size());
        for (const auto& message : messages) {
            hash.Mix(message.is8Bit);
            hash.Mix(message.text.size());
            for (char16_t c : message.text) hash.Mix(c);
        }
    }
    return hash.value;
}

}  // namespace FrontendEventDriver

using FrontendEventDriver::Argument;
using FrontendEventDriver::Message;

static std::vector<Message> captured;

static int APS5_VABI LocalConnection(FrontendChannel*) {
    return 1;
}

static void APS5_VABI CaptureMessage(FrontendChannel*, const String* message) {
    Message copy{true, {}};
    if (message && message->impl) {
        copy.is8Bit = (message->impl->hashAndFlags & 4) != 0;
        for (unsigned i = 0; i < message->impl->length; ++i)
            copy.text.push_back(copy.is8Bit ? static_cast<const unsigned char*>(message->impl->data)[i] : static_cast<const char16_t*>(message->impl->data)[i]);
    }
    captured.push_back(copy);
}

static void APS5_VABI NoDestructor(FrontendChannel*) {}

static void* const ChannelVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&NoDestructor),
    reinterpret_cast<void*>(&NoDestructor),
    reinterpret_cast<void*>(&LocalConnection),
    reinterpret_cast<void*>(&CaptureMessage),
};

static String MakeString(const Argument& argument) {
    if (argument.isNull) return String();
    if (argument.is8Bit) {
        std::string bytes(argument.text.begin(), argument.text.end());
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<unsigned>(bytes.size()));
    }
    return _ZN3WTF10StringImpl6createEPKDsj(argument.text.data(), static_cast<unsigned>(argument.text.size()));
}

struct ExportAdapter {
    FrontendRouterRef router = _ZN9Inspector14FrontendRouter6createEv();
    FrontendChannel channel{&ChannelVtable[2]};
    ExportAdapter() { _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &channel); }
    ~ExportAdapter() { _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router.ptr, &channel); }

    std::vector<Message> Call(int index, const Argument* args) {
        const FrontendDispatcher dispatcher{router.ptr};
        String strings[5];
        for (int i = 0; i < 5; ++i) strings[i] = MakeString(args[i]);
        captured.clear();
        switch (index) {
    case 0: _ZN9Inspector21CSSFrontendDispatcher17styleSheetChangedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 1: _ZN9Inspector21CSSFrontendDispatcher17styleSheetRemovedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 2: _ZN9Inspector21CSSFrontendDispatcher23mediaQueryResultChangedEv(&dispatcher); break;
    case 3: _ZN9Inspector21DOMFrontendDispatcher15documentUpdatedEv(&dispatcher); break;
    case 4: _ZN9Inspector21DOMFrontendDispatcher16attributeRemovedEiRKN3WTF6StringE(&dispatcher, args[0].integer, &strings[1]); break;
    case 5: _ZN9Inspector21DOMFrontendDispatcher16childNodeRemovedEii(&dispatcher, args[0].integer, args[1].integer); break;
    case 6: _ZN9Inspector21DOMFrontendDispatcher16shadowRootPoppedEii(&dispatcher, args[0].integer, args[1].integer); break;
    case 7: _ZN9Inspector21DOMFrontendDispatcher17attributeModifiedEiRKN3WTF6StringES4_(&dispatcher, args[0].integer, &strings[1], &strings[2]); break;
    case 8: _ZN9Inspector21DOMFrontendDispatcher19didAddEventListenerEi(&dispatcher, args[0].integer); break;
    case 9: _ZN9Inspector21DOMFrontendDispatcher20pseudoElementRemovedEii(&dispatcher, args[0].integer, args[1].integer); break;
    case 10: _ZN9Inspector21DOMFrontendDispatcher21characterDataModifiedEiRKN3WTF6StringE(&dispatcher, args[0].integer, &strings[1]); break;
    case 11: _ZN9Inspector21DOMFrontendDispatcher21childNodeCountUpdatedEii(&dispatcher, args[0].integer, args[1].integer); break;
    case 12: _ZN9Inspector21DOMFrontendDispatcher23willRemoveEventListenerEi(&dispatcher, args[0].integer); break;
    case 13: _ZN9Inspector21DOMFrontendDispatcher25customElementStateChangedEiNS_8Protocol3DOM18CustomElementStateE(&dispatcher, args[0].integer, args[1].integer); break;
    case 14: _ZN9Inspector21DOMFrontendDispatcher34powerEfficientPlaybackStateChangedEidb(&dispatcher, args[0].integer, args[1].number, args[2].boolean); break;
    case 15: _ZN9Inspector21DOMFrontendDispatcher7inspectEi(&dispatcher, args[0].integer); break;
    case 16: _ZN9Inspector22HeapFrontendDispatcher16trackingCompleteEdRKN3WTF6StringE(&dispatcher, args[0].number, &strings[1]); break;
    case 17: _ZN9Inspector22PageFrontendDispatcher13frameDetachedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 18: _ZN9Inspector22PageFrontendDispatcher14loadEventFiredEd(&dispatcher, args[0].number); break;
    case 19: _ZN9Inspector22PageFrontendDispatcher19frameStartedLoadingERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 20: _ZN9Inspector22PageFrontendDispatcher19frameStoppedLoadingERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 21: _ZN9Inspector22PageFrontendDispatcher20domContentEventFiredEd(&dispatcher, args[0].number); break;
    case 22: _ZN9Inspector22PageFrontendDispatcher24frameScheduledNavigationERKN3WTF6StringEd(&dispatcher, &strings[0], args[1].number); break;
    case 23: _ZN9Inspector24CanvasFrontendDispatcher13canvasRemovedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 24: _ZN9Inspector24CanvasFrontendDispatcher16extensionEnabledERKN3WTF6StringES4_(&dispatcher, &strings[0], &strings[1]); break;
    case 25: _ZN9Inspector24CanvasFrontendDispatcher16recordingStartedERKN3WTF6StringENS_8Protocol9Recording9InitiatorE(&dispatcher, &strings[0], args[1].integer); break;
    case 26: _ZN9Inspector24CanvasFrontendDispatcher18clientNodesChangedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 27: _ZN9Inspector24CanvasFrontendDispatcher19canvasMemoryChangedERKN3WTF6StringEd(&dispatcher, &strings[0], args[1].number); break;
    case 28: _ZN9Inspector24TargetFrontendDispatcher15targetDestroyedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 29: _ZN9Inspector24TargetFrontendDispatcher25dispatchMessageFromTargetERKN3WTF6StringES4_(&dispatcher, &strings[0], &strings[1]); break;
    case 30: _ZN9Inspector24TargetFrontendDispatcher26didCommitProvisionalTargetERKN3WTF6StringES4_(&dispatcher, &strings[0], &strings[1]); break;
    case 31: _ZN9Inspector24WorkerFrontendDispatcher13workerCreatedERKN3WTF6StringES4_S4_(&dispatcher, &strings[0], &strings[1], &strings[2]); break;
    case 32: _ZN9Inspector24WorkerFrontendDispatcher16workerTerminatedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 33: _ZN9Inspector24WorkerFrontendDispatcher25dispatchMessageFromWorkerERKN3WTF6StringES4_(&dispatcher, &strings[0], &strings[1]); break;
    case 34: _ZN9Inspector25NetworkFrontendDispatcher12dataReceivedERKN3WTF6StringEdii(&dispatcher, &strings[0], args[1].number, args[2].integer, args[3].integer); break;
    case 35: _ZN9Inspector25NetworkFrontendDispatcher15webSocketClosedERKN3WTF6StringEd(&dispatcher, &strings[0], args[1].number); break;
    case 36: _ZN9Inspector25NetworkFrontendDispatcher16webSocketCreatedERKN3WTF6StringES4_(&dispatcher, &strings[0], &strings[1]); break;
    case 37: _ZN9Inspector25NetworkFrontendDispatcher19webSocketFrameErrorERKN3WTF6StringEdS4_(&dispatcher, &strings[0], args[1].number, &strings[2]); break;
    case 38: _ZN9Inspector26DebuggerFrontendDispatcher19globalObjectClearedEv(&dispatcher); break;
    case 39: _ZN9Inspector26DebuggerFrontendDispatcher19scriptFailedToParseERKN3WTF6StringES4_iiS4_(&dispatcher, &strings[0], &strings[1], args[2].integer, args[3].integer, &strings[4]); break;
    case 40: _ZN9Inspector26DebuggerFrontendDispatcher25playBreakpointActionSoundEi(&dispatcher, args[0].integer); break;
    case 41: _ZN9Inspector26DebuggerFrontendDispatcher7resumedEv(&dispatcher); break;
    case 42: _ZN9Inspector26TimelineFrontendDispatcher16recordingStartedEd(&dispatcher, args[0].number); break;
    case 43: _ZN9Inspector26TimelineFrontendDispatcher16recordingStoppedEd(&dispatcher, args[0].number); break;
    case 44: _ZN9Inspector26TimelineFrontendDispatcher18autoCaptureStartedEv(&dispatcher); break;
    case 45: _ZN9Inspector27AnimationFrontendDispatcher13targetChangedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 46: _ZN9Inspector27AnimationFrontendDispatcher13trackingStartEd(&dispatcher, args[0].number); break;
    case 47: _ZN9Inspector27AnimationFrontendDispatcher16trackingCompleteEd(&dispatcher, args[0].number); break;
    case 48: _ZN9Inspector27AnimationFrontendDispatcher18animationDestroyedERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 49: _ZN9Inspector27InspectorFrontendDispatcher25evaluateForTestInFrontendERKN3WTF6StringE(&dispatcher, &strings[0]); break;
    case 50: _ZN9Inspector27LayerTreeFrontendDispatcher18layerTreeDidChangeEv(&dispatcher); break;
    case 51: _ZN9Inspector32ScriptProfilerFrontendDispatcher13trackingStartEd(&dispatcher, args[0].number); break;
    case 52: _ZN9Inspector34ApplicationCacheFrontendDispatcher19networkStateUpdatedEb(&dispatcher, args[0].boolean); break;
    case 53: _ZN9Inspector34ApplicationCacheFrontendDispatcher29applicationCacheStatusUpdatedERKN3WTF6StringES4_i(&dispatcher, &strings[0], &strings[1], args[2].integer); break;
        default: std::abort();
        }
        return captured;
    }
};

static const std::uint64_t ExpectedHashes[] = {
    0xafbcaaf5e367b0f2ULL,
    0xe0c743f50def49acULL,
    0x31261db12d4f9018ULL,
    0x35bef9325372201cULL,
    0x861f50e58aa445d8ULL,
    0xd27d3f8f64f01cd9ULL,
    0xcbcb96c3cb684de5ULL,
    0xcace51bfb025db69ULL,
    0xd4433086d8e57315ULL,
    0x14cf757f696d649aULL,
    0x0d9dab36b5e23989ULL,
    0x27b6a7abd0729fb3ULL,
    0x6ba9974a4209a2cdULL,
    0xb27c5c12334e5880ULL,
    0x247f4b0fe17f1838ULL,
    0x063af61654ad8943ULL,
};

static std::string Ascii(const Message& message) {
    return std::string(message.text.begin(), message.text.end());
}

static void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= 16; ++seed) {
        ExportAdapter adapter;
        Require(FrontendEventDriver::Run(adapter, seed * 2654435761u, 1000) == ExpectedHashes[seed - 1]);
    }
}

static void TestExamples() {
    ExportAdapter adapter;
    const FrontendDispatcher dispatcher{adapter.router.ptr};
    captured.clear();
    _ZN9Inspector21DOMFrontendDispatcher15documentUpdatedEv(&dispatcher);
    Require(captured.size() == 1 && captured[0].is8Bit && Ascii(captured[0]) == "{\"method\":\"DOM.documentUpdated\"}");

    captured.clear();
    const String name = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("a\"b\n"), 4);
    const String value;
    _ZN9Inspector21DOMFrontendDispatcher17attributeModifiedEiRKN3WTF6StringES4_(&dispatcher, -7, &name, &value);
    Require(Ascii(captured[0]) == "{\"method\":\"DOM.attributeModified\",\"params\":{\"nodeId\":-7,\"name\":\"a\\\"b\\n\",\"value\":\"\"}}");

    captured.clear();
    _ZN9Inspector21DOMFrontendDispatcher34powerEfficientPlaybackStateChangedEidb(&dispatcher, 3, 1.5, true);
    Require(Ascii(captured[0]) == "{\"method\":\"DOM.powerEfficientPlaybackStateChanged\",\"params\":{\"nodeId\":3,\"timestamp\":1.5,\"isPowerEfficient\":true}}");

    captured.clear();
    _ZN9Inspector22PageFrontendDispatcher14loadEventFiredEd(&dispatcher, std::numeric_limits<double>::infinity());
    Require(Ascii(captured[0]) == "{\"method\":\"Page.loadEventFired\",\"params\":{\"timestamp\":null}}");

    captured.clear();
    const String canvas = _ZN3WTF10StringImpl6createEPKDsj(u"c\xD800", 2);
    _ZN9Inspector24CanvasFrontendDispatcher16recordingStartedERKN3WTF6StringENS_8Protocol9Recording9InitiatorE(&dispatcher, &canvas, 224);
    Require(!captured[0].is8Bit && Ascii(captured[0]) == "{\"method\":\"Canvas.recordingStarted\",\"params\":{\"canvasId\":\"c\\ud800\",\"initiator\":\"auto-capture\"}}");

    captured.clear();
    Require(Throws<std::out_of_range>([&] { _ZN9Inspector24CanvasFrontendDispatcher16recordingStartedERKN3WTF6StringENS_8Protocol9Recording9InitiatorE(&dispatcher, &canvas, 225); }));
    Require(Throws<std::out_of_range>([&] { _ZN9Inspector21DOMFrontendDispatcher25customElementStateChangedEiNS_8Protocol3DOM18CustomElementStateE(&dispatcher, 1, 111); }));
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector21DOMFrontendDispatcher15documentUpdatedEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector22PageFrontendDispatcher13frameDetachedERKN3WTF6StringE(&dispatcher, nullptr); }));
    Require(captured.empty());
}

int main() {
    TestTranscripts();
    TestExamples();
    return 0;
}
