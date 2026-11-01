#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
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

int main() {
    bool passed = ReportsNotImplemented("sceVideodec2TermLibrary", [] { return sceVideodec2TermLibrary(); });
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
