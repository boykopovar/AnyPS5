#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

std::set<void*> live;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer)
        throw std::bad_alloc();
    live.insert(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer)
        return;
    if (live.erase(pointer) != 1)
        std::abort();
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return 0;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RtpTransceiverInit check failed at line %u\n", location.line());
    std::abort();
}

template<typename TCall>
bool ThrowsInvalid(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename TValue>
TValue* Allocate(std::size_t count = 1) {
    auto* result = static_cast<TValue*>(_Znwm_nid_postfix(sizeof(TValue) * count));
    std::memset(result, 0, sizeof(TValue) * count);
    return result;
}

void InitString(WebRtcGuestString& target, std::string_view text) {
    target.reserved = 0;
    target.size = text.size();
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        std::memset(target.buffer, 0, sizeof(target.buffer));
        std::memcpy(target.buffer, text.data(), text.size());
        target.capacity = WebRtcGuestStringInlineCapacity;
        return;
    }
    target.capacity = text.size() | WebRtcGuestStringInlineCapacity;
    target.pointer = static_cast<char*>(GuestStringStorage::Allocate(target.capacity + 1));
    std::memcpy(target.pointer, text.data(), text.size());
    target.pointer[text.size()] = '\0';
}

WebRtcStringMapNode* EndNode(WebRtcStringMap& map) {
    return reinterpret_cast<WebRtcStringMapNode*>(&map.root);
}

void TestDefaultConstructorAndDestructor() {
    WebRtcRtpTransceiverInit init;
    std::memset(&init, 0xA5, sizeof(init));
    _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(&init);
    Require(init.direction == WebRtcRtpTransceiverDirectionSendRecv);
    Require(!init.streamIds.begin && !init.streamIds.end && !init.streamIds.capacityEnd);
    Require(!init.sendEncodings.begin && !init.sendEncodings.end && !init.sendEncodings.capacityEnd);
    _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init);
    Require(live.empty());
}

void TestPopulatedDestructor() {
    WebRtcRtpTransceiverInit init{};
    init.streamIds.begin = Allocate<WebRtcGuestString>(2);
    init.streamIds.end = init.streamIds.begin + 2;
    init.streamIds.capacityEnd = init.streamIds.end;
    InitString(init.streamIds.begin[0], "stream");
    InitString(init.streamIds.begin[1], "stream identifier beyond inline storage");

    init.sendEncodings.begin = Allocate<WebRtcRtpEncodingParameters>();
    init.sendEncodings.end = init.sendEncodings.begin + 1;
    init.sendEncodings.capacityEnd = init.sendEncodings.end;
    auto& encoding = *init.sendEncodings.begin;
    encoding.scalabilityMode.engaged = 1;
    InitString(encoding.scalabilityMode.value, "scalability mode beyond inline storage");
    InitString(encoding.rid, "encoding identifier beyond inline storage");
    encoding.codec.engaged = 1;
    InitString(encoding.codec.value.name, "codec name beyond inline storage");
    encoding.codec.value.rtcpFeedback.begin = Allocate<WebRtcRtcpFeedback>(2);
    encoding.codec.value.rtcpFeedback.end = encoding.codec.value.rtcpFeedback.begin + 2;
    encoding.codec.value.rtcpFeedback.capacityEnd = encoding.codec.value.rtcpFeedback.end;
    auto* node = Allocate<WebRtcStringMapNode>();
    node->links.parent = EndNode(encoding.codec.value.parameters);
    InitString(node->key, "parameter name beyond inline storage");
    InitString(node->value, "parameter value beyond inline storage");
    encoding.codec.value.parameters.beginNode = node;
    encoding.codec.value.parameters.root = node;
    encoding.codec.value.parameters.size = 1;

    _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init);
    Require(live.empty());
}

void TestInvalidInputsThrowBeforeRelease() {
    Require(ThrowsInvalid([] { _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([] { _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(nullptr); }));
    WebRtcRtpTransceiverInit init{};
    init.streamIds.begin = Allocate<WebRtcGuestString>();
    init.streamIds.end = init.streamIds.begin + 1;
    init.streamIds.capacityEnd = init.streamIds.begin;
    Require(ThrowsInvalid([&] { _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(&init); }));
    _ZdlPv_nid_postfix(init.streamIds.begin);
    Require(live.empty());
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        address = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Require(address != nullptr);
#else
        address = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(address != MAP_FAILED);
#endif
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, 4096);
#endif
    }
    template<typename TValue>
    TValue* As() const {
        return static_cast<TValue*>(address);
    }

private:
    void* address = nullptr;
};

void TestGuardedObjectThrows() {
    GuardedPage page;
    Require(ThrowsInvalid([&] { _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(page.As<WebRtcRtpTransceiverInit>()); }));
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestDefaultConstructorAndDestructor();
    TestPopulatedDestructor();
    TestInvalidInputsThrowBeforeRelease();
    TestGuardedObjectThrows();
    Require(live.empty());
    std::puts("libwebrtc RtpTransceiverInit tests passed");
    return 0;
}
