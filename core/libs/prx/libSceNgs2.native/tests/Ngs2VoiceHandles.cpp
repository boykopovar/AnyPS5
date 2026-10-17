#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

static constexpr std::array<std::string_view, 6> Names{"control", "commands", "state", "port", "query", "flags"};
static constexpr int InvalidVoice = static_cast<int>(0x804A8203u);
static constexpr int NullList = static_cast<int>(0x804A8360u);

static void Status(int actual, int expected, const std::string& message) {
    if (actual != expected) throw std::runtime_error(message);
}

static void Invalid(std::string_view api, uintptr_t handle) {
    alignas(16) std::array<unsigned char, 128> output;
    output.fill(0xa5);
    const Ngs2VoiceEventParam event{{sizeof(Ngs2VoiceEventParam), 0, SCE_NGS2_VOICE_PARAM_EVENT}, SCE_NGS2_VOICE_EVENT_STOP_IMM};
    const Ngs2VoiceCommand command{2, 0, 4, 0, {.u = SCE_NGS2_VOICE_EVENT_STOP_IMM}};
    int result = 0;
    if (api == "control") result = sceNgs2VoiceControl(handle, &event.header);
    else if (api == "commands") result = sceNgs2VoiceRunCommands(handle, &command, 1);
    else if (api == "state") result = sceNgs2VoiceGetState(handle, reinterpret_cast<Ngs2VoiceState*>(output.data()), sizeof(Ngs2SamplerVoiceState));
    else if (api == "port") result = sceNgs2VoiceGetPortInfo(handle, 0, reinterpret_cast<Ngs2VoicePortInfo*>(output.data()), sizeof(Ngs2VoicePortInfo));
    else if (api == "query") result = sceNgs2VoiceQueryInfo(handle, SCE_NGS2_VOICE_INFO_CHANNELS, output.data(), sizeof(Ngs2VoiceChannelsInfo));
    else if (api == "flags") result = sceNgs2VoiceGetStateFlags(handle, reinterpret_cast<std::uint32_t*>(output.data()));
    else throw std::runtime_error("unknown voice API test selection");
    Status(result, InvalidVoice, std::string(api) + " returned the wrong invalid-voice error");
    Require(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0xa5; }));
}

int main(int argc, char** argv) {
    try {
        if (argc > 2 || (argc == 2 && std::find(Names.begin(), Names.end(), std::string_view(argv[1])) == Names.end())) throw std::runtime_error("unknown voice API test selection");
        const auto selected = [&](auto name) { return argc == 1 || std::string_view(argv[1]) == name; };
        for (auto name : Names) if (selected(name)) { Invalid(name, 0); Invalid(name, 0x1234); }
        Status(sceNgs2VoiceControl(0x1234, nullptr), NullList, "Control parameter-list precedence differs from PS5");
        Status(sceNgs2VoiceGetState(0x1234, nullptr, 0), InvalidVoice, "GetState checked output before the voice");
        const auto system = CreateSystem();
        const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
        const auto voice = Voice(rack);
        for (auto name : Names) if (selected(name)) { Invalid(name, system); Invalid(name, rack); }
        Status(sceNgs2VoiceControl(voice, nullptr), NullList, "live voice lost the null parameter-list error");
        Status(sceNgs2VoiceGetState(voice, nullptr, 0), SCE_NGS2_ERROR_INVALID_OUT_SIZE, "GetState checked null output before its minimum size");
        Status(sceNgs2VoiceGetState(voice, nullptr, sizeof(Ngs2VoiceState) - 1), SCE_NGS2_ERROR_INVALID_OUT_SIZE, "GetState lost its minimum-size boundary");
        Status(sceNgs2VoiceGetState(voice, nullptr, sizeof(Ngs2VoiceState)), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "GetState rejected its minimum-size boundary before null output");
        Status(sceNgs2VoiceGetState(voice, nullptr, sizeof(Ngs2SamplerVoiceState)), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "GetState lost null-output validation");
        Status(sceNgs2VoiceGetStateFlags(voice, nullptr), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "GetStateFlags lost null-output validation");
        std::uint32_t flags = 0;
        Status(sceNgs2VoiceGetStateFlags(voice, &flags), SCE_NGS2_OK, "valid state-flags lookup failed");
        Ngs2SamplerVoiceState state{};
        Status(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)), SCE_NGS2_OK, "valid sampler state lookup failed");
        const Ngs2VoiceCommand stop{2, 0, 4, 0, {.u = SCE_NGS2_VOICE_EVENT_STOP_IMM}};
        Status(sceNgs2VoiceRunCommands(voice, &stop, 1), SCE_NGS2_OK, "valid voice command failed");
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
        for (auto name : Names) if (selected(name)) Invalid(name, voice);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        std::cout << "NGS2 invalid voice errors, outputs and precedence checked\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
