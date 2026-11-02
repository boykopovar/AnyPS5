#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

static bool ReportsNotImplemented(const std::string& name, const std::function<int()>& call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        if (error.what() == name + " not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", name.c_str(), error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", name.c_str());
    return false;
}

static bool ReportsReason(const std::string& name, const std::string& reason, const std::function<int()>& call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        if (error.what() == name + " (" + reason + ") not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", name.c_str(), error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting why it is not implemented\n", name.c_str());
    return false;
}

static bool TwoPictureQueriesNameTheirReason() {
    alignas(256) static std::array<std::uint8_t, 0x1000> frame{};
    alignas(8) std::array<std::uint8_t, 0x58> first{};
    alignas(8) std::array<std::uint8_t, 0x58> second{};
    const std::uint64_t size = first.size();
    std::memcpy(first.data(), &size, sizeof(size));
    std::memcpy(second.data(), &size, sizeof(size));
    SceVideodec2::OutputInfo output{};
    output.thisSize = sizeof(output);
    output.pictureCount = 2;
    output.frameBuffer = frame.data();
    output.frameBufferSize = frame.size();
    auto* firstInfo = reinterpret_cast<SceVideodec2::AvcPictureInfo*>(first.data());
    auto* secondInfo = reinterpret_cast<SceVideodec2::AvcPictureInfo*>(second.data());
    bool passed = ReportsReason("sceVideodec2GetAvcPictureInfo_nid_postfix", "a query for two pictures",
                                [&] { return sceVideodec2GetAvcPictureInfo_nid_postfix(&output, firstInfo, secondInfo); });
    passed = ReportsReason("sceVideodec2GetPictureInfo_nid_postfix", "a query for two pictures",
                           [&] { return sceVideodec2GetPictureInfo_nid_postfix(&output, firstInfo, secondInfo); }) && passed;
    return passed;
}

int main() {
    bool passed = TwoPictureQueriesNameTheirReason();
    passed = ReportsNotImplemented("sceVideodec2TermLibrary", [] { return sceVideodec2TermLibrary(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2QueryLibraryInfo", [] { return sceVideodec2QueryLibraryInfo(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2QueryDecoderInfo", [] { return sceVideodec2QueryDecoderInfo(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2MapMemory", [] { return sceVideodec2MapMemory(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2MapDirectMemory", [] { return sceVideodec2MapDirectMemory(nullptr, nullptr); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2PresetDecoderConfigInfo", [] { return sceVideodec2PresetDecoderConfigInfo(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2PresetDecoderConfigInfoAvc", [] { return sceVideodec2PresetDecoderConfigInfoAvc(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2PresetDecoderConfigInfoHevc", [] { return sceVideodec2PresetDecoderConfigInfoHevc(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2PresetDecoderConfigInfoVp9", [] { return sceVideodec2PresetDecoderConfigInfoVp9(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2CreateDecoderBid", [] { return sceVideodec2CreateDecoderBid(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2CreateHevcDecoder", [] { return sceVideodec2CreateHevcDecoder(); }) && passed;
    passed = ReportsNotImplemented("sceVideodec2QueryHevcDecoderMemoryInfo", [] { return sceVideodec2QueryHevcDecoderMemoryInfo(); }) && passed;
    return passed ? 0 : 1;
}
