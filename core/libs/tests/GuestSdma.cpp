#include "prx/libSceSdma/libSceSdma.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

std::uint64_t Address(const void* pointer) { return reinterpret_cast<std::uint64_t>(pointer); }

constexpr int NotQueueMode = static_cast<int>(0x80990002);
constexpr int InvalidArgument = static_cast<int>(0x80990005);
constexpr int Busy = static_cast<int>(0x80990006);
constexpr int RingFull = static_cast<int>(0x80990064);
constexpr int FencePending = static_cast<int>(0x80990065);

void Require(bool value) {
    if (!value) std::abort();
}

template <typename TFunction>
void RequireThrows(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    std::abort();
}

struct Engine {
    std::vector<std::uint8_t> buffer;
    void* handle = nullptr;
    Engine(std::uint32_t engine, std::uint32_t size, std::uint64_t* writeBack = nullptr) : buffer(size) {
        SdmaOpenParam param{engine, 0, 0, reinterpret_cast<std::uint64_t>(writeBack), 0, writeBack ? 0x10u : 0u};
        Require(sceSdmaOpen(&handle, buffer.data(), size, &param) == 0 && handle);
    }
    ~Engine() { Require(sceSdmaClose(handle) == 0); }
};

std::vector<std::uint8_t> Pattern(std::size_t size, std::uint8_t seed) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = static_cast<std::uint8_t>(index * 31 + seed);
    return bytes;
}

void RequireOpen() {
    std::vector<std::uint8_t> buffer(0x8000);
    SdmaOpenParam param{12, 0, 0, 0, 0, 0};
    void* handle = nullptr;
    Require(sceSdmaOpen(&handle, buffer.data(), 0x8000, nullptr) == InvalidArgument);
    Require(sceSdmaOpen(&handle, buffer.data(), 0x7fff, &param) == InvalidArgument);
    Require(sceSdmaOpen(&handle, nullptr, 0x8000, &param) == InvalidArgument);
    for (std::uint32_t engine : {0u, 11u, 20u, 0xffffffffu}) {
        param.engine = engine;
        Require(sceSdmaOpen(&handle, buffer.data(), 0x8000, &param) == InvalidArgument);
    }
    Require(handle == nullptr);
    for (std::uint32_t engine = 12; engine < 20; ++engine) {
        param.engine = engine;
        void* first = nullptr;
        void* second = nullptr;
        Require(sceSdmaOpen(&first, buffer.data(), 0x8000, &param) == 0 && first);
        Require(sceSdmaOpen(&second, buffer.data(), 0x8000, &param) == Busy && !second);
        Require(sceSdmaClose(first) == 0);
        Require(sceSdmaOpen(&second, buffer.data(), 0x8000, &param) == 0 && second == first);
        Require(sceSdmaClose(second) == 0);
    }
}

void RequireLinear() {
    Engine engine(13, 0x8000);
    const auto source = Pattern(0x400000 + 0x1234, 7);
    std::vector<std::uint8_t> destination(source.size() + 16, 0xEE);
    Require(sceSdmaCopyLinear(engine.handle, Address(source.data()), Address(destination.data() + 8), static_cast<std::uint32_t>(source.size())) == 0);
    Require(std::memcmp(destination.data() + 8, source.data(), source.size()) == 0);
    Require(destination[7] == 0xEE && destination[source.size() + 8] == 0xEE);
    Require(sceSdmaCopyLinear(engine.handle, Address(nullptr), Address(nullptr), 0) == 0);
    std::vector<std::uint8_t> small(64, 0);
    Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source.data()), Address(small.data()), 64) == 0);
    Require(std::memcmp(small.data(), source.data(), 64) == 0);
    RequireThrows([&] { sceSdmaCopyLinear(engine.handle, Address(nullptr), Address(nullptr), 0xffffffffu); });
    Require(sceSdmaFlushAll(engine.handle) == 0);
}

void RequireFill() {
    Engine engine(14, 0x8000);
    std::vector<std::uint8_t> bytes(0x400000 + 0x40, 0x11);
    Require(sceSdmaConstantFill(engine.handle, 0, Address(bytes.data() + 1), 0x400000 + 0x3e, 0) == 0);
    Require(bytes[0] == 0x11 && bytes[0x400000 + 0x3f] == 0x11 && bytes[1] == 0 && bytes[0x400000 + 0x3e] == 0);
    Require(sceSdmaConstantFillNonBlocking(engine.handle, 0x5a5a5a5au, Address(bytes.data() + 3), 5, 0) == 0);
    Require(bytes[2] == 0 && bytes[3] == 0x5a && bytes[7] == 0x5a && bytes[8] == 0);
    alignas(4) std::uint32_t words[6]{};
    Require(sceSdmaConstantFill(engine.handle, 0x11223344u, Address(words + 1), 16, 1) == 0);
    Require(words[0] == 0 && words[1] == 0x11223344u && words[4] == 0x11223344u && words[5] == 0);
    RequireThrows([&] { sceSdmaConstantFill(engine.handle, 0x11223344u, Address(words), 16, 0); });
    RequireThrows([&] { sceSdmaConstantFill(engine.handle, 0x11223344u, Address(words), 6, 1); });
}

void RequireWindow() {
    Engine engine(15, 0x8000);
    const std::uint32_t sourcePitch = 40, destinationPitch = 24;
    auto source = Pattern(sourcePitch * 20 * 4, 3);
    std::vector<std::uint8_t> destination(destinationPitch * 16 * 4, 0xCC);
    const auto before = destination;
    Require(sceSdmaCopyWindowL2L(engine.handle, Address(source.data()), Address(destination.data()), 32, 5, 3, sourcePitch, 20, 7, 4, destinationPitch, 16, 2, 9) == 0);
    for (std::uint32_t y = 0; y < 16; ++y)
        for (std::uint32_t x = 0; x < destinationPitch; ++x) {
            const std::uint8_t* got = destination.data() + (y * destinationPitch + x) * 4;
            const bool inside = x >= 2 && x < 7 && y >= 9 && y < 12;
            const std::uint8_t* want = inside ? source.data() + ((y - 9 + 4) * sourcePitch + (x - 2 + 7)) * 4 : before.data() + (y * destinationPitch + x) * 4;
            Require(std::memcmp(got, want, 4) == 0);
        }
    std::vector<std::uint8_t> narrow(64, 0);
    Require(sceSdmaCopyWindowL2LNonBlocking(engine.handle, Address(source.data()), Address(narrow.data()), 24, 3, 2, 8, 2, 1, 0, 8, 2, 0, 1) == 0);
    Require(narrow[8] == source[1] && narrow[10] == source[3] && narrow[11] == 0 && narrow[16] == source[9] && narrow[0] == 0);
    Require(sceSdmaCopyWindowL2L(engine.handle, Address(source.data()), Address(narrow.data()), 128, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0) == 0);
    Require(std::memcmp(narrow.data(), source.data(), 16) == 0);
    RequireThrows([&] { sceSdmaCopyWindowL2L(engine.handle, Address(source.data()), Address(narrow.data()), 32, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0); });
    RequireThrows([&] { sceSdmaCopyWindowL2L(engine.handle, Address(source.data()), Address(narrow.data()), 32, 1, 1, 1, 1, 0x4000, 0, 1, 1, 0, 0); });
    RequireThrows([&] { sceSdmaCopyWindowL2L(engine.handle, Address(source.data()), Address(narrow.data()), 32, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0); });
}

void RequireQueue() {
    std::uint64_t writeBack = 0xdeadbeef;
    Engine engine(16, 0x8000, &writeBack);
    const auto source = Pattern(64, 9);
    std::vector<std::uint8_t> destination(64, 0);
    Require(sceSdmaCancelQueue(engine.handle) == NotQueueMode);
    Require(sceSdmaFlushAll(engine.handle) == 0 && writeBack == 0xdeadbeef);
    Require(sceSdmaCopyLinear(engine.handle, Address(source.data()), Address(destination.data()), 16) == 0);
    Require(writeBack == (7 + 4) * 4);

    Require(sceSdmaBeginQueueMode(engine.handle) == 0);
    Require(writeBack == (7 + 4 + 4) * 4);
    Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source.data() + 16), Address(destination.data() + 16), 16) == 0);
    Require(sceSdmaFenceNonBlocking(engine.handle) == 0);
    Require(destination[16] == 0 && sceSdmaTryFlush(engine.handle) == FencePending);
    Require(sceSdmaKickQueue(engine.handle) == 0);
    Require(std::memcmp(destination.data() + 16, source.data() + 16, 16) == 0);
    Require(writeBack == (15 + 7 + 4) * 4);
    Require(sceSdmaTryFlush(engine.handle) == 0 && sceSdmaTryFlush(engine.handle) == 0);

    Require(sceSdmaConstantFillNonBlocking(engine.handle, 0xffffffffu, Address(destination.data() + 32), 8, 0) == 0);
    Require(sceSdmaFenceNonBlocking(engine.handle) == 0);
    Require(sceSdmaCancelQueue(engine.handle) == 0);
    Require(destination[32] == 0 && sceSdmaTryFlush(engine.handle) == 0);
    Require(sceSdmaKickQueue(engine.handle) == 0 && destination[32] == 0);
    Require(writeBack == (26) * 4);

    Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source.data() + 32), Address(destination.data() + 32), 8) == 0);
    Require(sceSdmaCopyLinear(engine.handle, Address(source.data() + 40), Address(destination.data() + 40), 8) == 0);
    Require(std::memcmp(destination.data() + 32, source.data() + 32, 16) == 0);

    Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source.data() + 48), Address(destination.data() + 48), 16) == 0);
    Require(destination[48] == 0);
    Require(sceSdmaEndQueueMode(engine.handle) == 0);
    Require(std::memcmp(destination.data() + 48, source.data() + 48, 16) == 0);
    Require(sceSdmaCancelQueue(engine.handle) == NotQueueMode);
}

void RequireRing() {
    Engine engine(17, 0x8000);
    std::uint8_t source[8]{1, 2, 3, 4, 5, 6, 7, 8};
    std::uint8_t destination[8]{};
    Require(sceSdmaBeginQueueMode(engine.handle) == 0);
    for (int index = 0; index < 582; ++index) Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source), Address(destination), 8) == 0);
    Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source), Address(destination), 8) == RingFull);
    Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source), Address(destination), 0) == RingFull);
    Require(sceSdmaConstantFillNonBlocking(engine.handle, 0, Address(destination), 0, 0) == 0);
    Require(sceSdmaCopyWindowL2LNonBlocking(engine.handle, Address(source), Address(destination), 8, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0) == RingFull);
    Require(sceSdmaConstantFillNonBlocking(engine.handle, 0, Address(destination), 8, 0) == 0);
    Require(sceSdmaFenceNonBlocking(engine.handle) == 0);
    Require(sceSdmaFenceNonBlocking(engine.handle) == 0);
    Require(sceSdmaFenceNonBlocking(engine.handle) == RingFull);
    Require(sceSdmaTryFlush(engine.handle) == FencePending);
    Require(destination[0] == 0);
    Require(sceSdmaCopyLinear(engine.handle, Address(source), Address(destination), 8) == 0);
    Require(std::memcmp(destination, source, 8) == 0);
    Require(sceSdmaTryFlush(engine.handle) == 0);
    Require(sceSdmaCopyLinearNonBlocking(engine.handle, Address(source), Address(destination), 8) == 0);
    Require(sceSdmaEndQueueMode(engine.handle) == 0);

    Engine large(18, 0x40000);
    for (int index = 0; index < 4095; ++index) Require(sceSdmaFenceNonBlocking(large.handle) == 0);
    Require(sceSdmaFenceNonBlocking(large.handle) == RingFull);
    Require(sceSdmaTryFlush(large.handle) == 0);
    Require(sceSdmaFenceNonBlocking(large.handle) == 0);
    Require(sceSdmaFlushAll(large.handle) == 0);

    {
        Engine exact(19, 0x8000);
        std::vector<std::uint8_t> big(0x400000, 0x3c), target(0x400000, 0);
        Require(sceSdmaBeginQueueMode(exact.handle) == 0);
        for (int index = 0; index < 581; ++index) Require(sceSdmaCopyLinearNonBlocking(exact.handle, Address(source), Address(destination), 1) == 0);
        Require(sceSdmaCopyLinearNonBlocking(exact.handle, Address(big.data()), Address(target.data()), 0x400000) == RingFull);
        Require(sceSdmaCopyLinearNonBlocking(exact.handle, Address(big.data()), Address(target.data()), 0x3fffff) == 0);
        Require(target[0] == 0);
        Require(sceSdmaEndQueueMode(exact.handle) == 0);
        Require(target[0x3ffffe] == 0x3c && target[0x3fffff] == 0);
    }

    {
        std::uint64_t writeBack = 0;
        Engine wrapped(19, 0x80000, &writeBack);
        Require(sceSdmaBeginQueueMode(wrapped.handle) == 0);
        for (int index = 0; index < 18137; ++index) Require(sceSdmaCopyLinearNonBlocking(wrapped.handle, Address(source), Address(destination), 1) == 0);
        Require(sceSdmaCopyLinearNonBlocking(wrapped.handle, Address(source), Address(destination), 1) == 0);
        RequireThrows([&] { sceSdmaCancelQueue(wrapped.handle); });
        Require(sceSdmaEndQueueMode(wrapped.handle) == 0);
        Require(writeBack == (18138 * 7 + 4) * 4);
    }
}

}

int main() {
    RequireOpen();
    RequireLinear();
    RequireFill();
    RequireWindow();
    RequireQueue();
    RequireRing();
}
