#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

static constexpr std::array<std::string_view, 7> Names{"control", "commands", "state", "port", "query", "flags", "owner"};
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
    else if (api == "owner") result = sceNgs2VoiceGetOwner(handle, reinterpret_cast<uintptr_t*>(output.data()), reinterpret_cast<std::uint32_t*>(output.data() + 16));
    else throw std::runtime_error("unknown voice API test selection");
    Status(result, InvalidVoice, std::string(api) + " returned the wrong invalid-voice error");
    Require(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0xa5; }));
}

template <typename TException>
static bool MatrixInfoThrows(uintptr_t voice, std::uint32_t matrixId) {
    Ngs2VoiceMatrixInfo info;
    std::memset(&info, 0xa5, sizeof(info));
    bool threw = false;
    try {
        sceNgs2VoiceGetMatrixInfo(voice, matrixId, &info, sizeof(info));

    } catch (const TException&) {
        threw = true;
    }
    const auto* bytes = reinterpret_cast<const unsigned char*>(&info);
    Require(std::all_of(bytes, bytes + sizeof(info), [](auto byte) { return byte == 0xa5; }));
    return threw;
}

static void MatrixInfo() {
    Ngs2VoiceMatrixInfo info;
    std::memset(&info, 0xa5, sizeof(info));
    Status(sceNgs2VoiceGetMatrixInfo(0x1234, 0, &info, sizeof(info)), InvalidVoice, "GetMatrixInfo accepted an invalid voice");
    const auto* bytes = reinterpret_cast<const unsigned char*>(&info);
    Require(std::all_of(bytes, bytes + sizeof(info), [](auto byte) { return byte == 0xa5; }));
    Status(sceNgs2VoiceGetMatrixInfo(0x1234, 0, nullptr, sizeof(info)), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "GetMatrixInfo checked the voice before null output");
    Status(sceNgs2VoiceGetMatrixInfo(0x1234, 0, &info, sizeof(info) - 1), SCE_NGS2_ERROR_INVALID_OUT_SIZE, "GetMatrixInfo checked the voice before the output size");
    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = Voice(rack);
    Status(sceNgs2VoiceGetMatrixInfo(voice, 0, &info, sizeof(info)), SCE_NGS2_OK, "GetMatrixInfo failed for a matrix without levels");
    Require(info.num_levels == 0 && std::all_of(std::begin(info.levels), std::end(info.levels), [](float level) { return level == 0.0f; }));
    const float levels[4] = {1.0f, 0.5f, -5.0f, 6.0f};
    Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 4, levels});
    std::memset(&info, 0xa5, sizeof(info));
    Status(sceNgs2VoiceGetMatrixInfo(voice, 0, &info, sizeof(info)), SCE_NGS2_OK, "GetMatrixInfo failed for a matrix with levels");
    Require(info.num_levels == 4 && info.levels[0] == 1.0f && info.levels[1] == 0.5f && info.levels[2] == -4.0f && info.levels[3] == 4.0f);
    Require(std::all_of(std::begin(info.levels) + 4, std::end(info.levels), [](float level) { return level == 0.0f; }));
    std::array<float, 64> full{};
    for (std::size_t i = 0; i < full.size(); i++) full[i] = static_cast<float>(i) / 64.0f;
    Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 64, full.data()});
    Status(sceNgs2VoiceGetMatrixInfo(voice, 0, &info, sizeof(info)), SCE_NGS2_OK, "GetMatrixInfo failed for 64 levels");
    Require(info.num_levels == 64 && std::equal(full.begin(), full.end(), info.levels));
    Status(sceNgs2VoiceGetMatrixInfo(voice, 0, &info, sizeof(info) + 1), SCE_NGS2_ERROR_INVALID_OUT_SIZE, "GetMatrixInfo accepted a larger output");
    Require(MatrixInfoThrows<std::invalid_argument>(voice, 1));
    const auto master = Voice(CreateRack(system, SCE_NGS2_RACK_ID_MASTERING));
    Require(MatrixInfoThrows<std::invalid_argument>(master, 0));
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Status(sceNgs2VoiceGetMatrixInfo(voice, 0, &info, sizeof(info)), InvalidVoice, "GetMatrixInfo accepted a voice of a destroyed rack");
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main(int argc, char** argv) {
    try {
        if (argc > 2 || (argc == 2 && std::find(Names.begin(), Names.end(), std::string_view(argv[1])) == Names.end())) throw std::runtime_error("unknown voice API test selection");
        const auto selected = [&](auto name) { return argc == 1 || std::string_view(argv[1]) == name; };
        for (auto name : Names) if (selected(name)) { Invalid(name, 0); Invalid(name, 0x1234); }
        Status(sceNgs2VoiceControl(0x1234, nullptr), NullList, "Control parameter-list precedence differs from PS5");
        Status(sceNgs2VoiceGetState(0x1234, nullptr, 0), InvalidVoice, "GetState checked output before the voice");
        Status(sceNgs2VoiceGetOwner(0x1234, nullptr, nullptr), InvalidVoice, "GetOwner checked output before the voice");
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
        uintptr_t owner = 0x5a5a;
        std::uint32_t index = 0x5a5a;
        Status(sceNgs2VoiceGetOwner(voice, nullptr, &index), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "GetOwner lost null-rack validation");
        Status(sceNgs2VoiceGetOwner(voice, &owner, nullptr), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "GetOwner lost null-index validation");
        Require(owner == 0x5a5a && index == 0x5a5a);
        const auto other = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
        for (const auto owned : {rack, other}) {
            Ngs2RackInfo info{};
            Status(sceNgs2RackGetInfo(owned, &info, sizeof(info)), SCE_NGS2_OK, "rack info lookup failed");
            Require(info.max_voices > 1);
            for (std::uint32_t voiceId = 0; voiceId < info.max_voices; voiceId++) {
                uintptr_t handle = 0;
                Status(sceNgs2RackGetVoiceHandle(owned, voiceId, &handle), SCE_NGS2_OK, "voice handle lookup failed");
                Status(sceNgs2VoiceGetOwner(handle, &owner, &index), SCE_NGS2_OK, "valid owner lookup failed");
                Require(owner == owned && index == voiceId);
            }
        }
        Require(sceNgs2RackDestroy(other, nullptr) == SCE_NGS2_OK);
        std::uint32_t flags = 0;
        Status(sceNgs2VoiceGetStateFlags(voice, &flags), SCE_NGS2_OK, "valid state-flags lookup failed");
        Ngs2SamplerVoiceState state{};
        Status(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)), SCE_NGS2_OK, "valid sampler state lookup failed");
        const Ngs2VoiceCommand stop{2, 0, 4, 0, {.u = SCE_NGS2_VOICE_EVENT_STOP_IMM}};
        Status(sceNgs2VoiceRunCommands(voice, &stop, 1), SCE_NGS2_OK, "valid voice command failed");
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
        for (auto name : Names) if (selected(name)) Invalid(name, voice);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        if (argc == 1) MatrixInfo();
        std::cout << "NGS2 invalid voice errors, outputs and precedence checked\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
