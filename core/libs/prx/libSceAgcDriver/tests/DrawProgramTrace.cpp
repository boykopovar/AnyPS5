#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawProgramTrace.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void EmptyTrace() {
    const auto trace = AgcDriver::DriverDetail::FormatDrawProgramTrace(12, 3, {});
    Check(trace == "[draw-programs] submission=12 queue=0x3 count=0", "empty draw trace changed shape");
}

void ProgramAddresses() {
    using namespace AgcDriver::DriverDetail;
    using ShaderRecompiler::ProgramRole;
    using ShaderRecompiler::ShaderStage;
    const DrawProgramTraceEntry entries[]{
        {ShaderStage::Vertex, ProgramRole::Main, 0x100000, 0x100040},
        {ShaderStage::Fragment, ProgramRole::Fragment, 0x200000, 0x200020},
    };
    const auto trace = FormatDrawProgramTrace(42, 0x9, entries);
    Check(trace == "[draw-programs] submission=42 queue=0x9 count=2 vertex/main=0x100000(entry=0x100040) fragment/fragment=0x200000(entry=0x200020)",
        "draw trace omitted the selected shader code or entry address");
}

}

int main() {
    try {
        EmptyTrace();
        ProgramAddresses();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
