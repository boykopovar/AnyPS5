#include "prx/libSceVoice/include/Api.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool Throws(TCall call, std::string_view message) {
    try {
        call();
    } catch (const std::logic_error& error) {
        return std::string_view(error.what()).find(message) != std::string_view::npos;
    }
    return false;
}

int main() {
    constexpr auto notInitialized = static_cast<std::int32_t>(0x804e0801u);
    constexpr auto invalidPort = static_cast<std::int32_t>(0x804e0804u);
    constexpr auto invalidArgument = static_cast<std::int32_t>(0x804e0805u);
    Require(sceVoiceGetPortAttr(128, 1001, nullptr, 1) == notInitialized);
    VoiceInitParam init{};
    init.app_type = 0x10000000;
    Require(sceVoiceInit(&init, 100) == 0);
    VoicePortParam param{};
    param.volume = 1.0f;
    std::array<unsigned char, 6> buffer{0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5};
    const auto original = buffer;
    Require(sceVoiceGetPortAttr(128, 1001, nullptr, 1) == invalidPort);
    Require(sceVoiceGetPortAttr(0, 1001, nullptr, 1) == invalidArgument);
    for (std::int32_t type = 0; type <= 5; ++type) {
        param = {};
        param.port_type = type;
        if (type == 1 || type == 3) {
            param.pcmaudio.buffer_size = 1024;
            param.pcmaudio.sample_rate = 16000;
        }
        if (type == 2 || type == 4) param.voice.bitrate = 16000;
        if (type == 5) param.device.type = 2;
        std::uint32_t port = 0;
        Require(sceVoiceCreatePort(&port, &param) == 0);
        for (const auto attr : {1000, 1001, 1002, 1003, 2000, 2001, 2002}) {
            const auto size = attr == 1003 ? 2 : attr == 1002 || attr == 2001 ? 4 : 1;
            Require(sceVoiceGetPortAttr(port, attr, buffer.data() + 1, size + 1) == invalidArgument);
            if (type == (attr == 1003 ? 5 : 0)) {
                Require(Throws([&] { sceVoiceGetPortAttr(port, attr, buffer.data() + 1, size); }, "device attribute backend"));
            } else {
                Require(sceVoiceGetPortAttr(port, attr, buffer.data() + 1, size) == invalidArgument);
            }
            Require(buffer == original);
        }
        Require(sceVoiceGetPortAttr(port, 1, buffer.data() + 1, 1) == invalidArgument);
        Require(sceVoiceDeletePort(port) == 0);
        Require(sceVoiceGetPortAttr(port, 1001, buffer.data() + 1, 1) == invalidPort);
    }
    Require(sceVoiceEnd_nid_postfix() == 0);
    return 0;
}
