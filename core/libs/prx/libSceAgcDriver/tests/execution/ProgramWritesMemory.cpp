#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t EndProgram = 0xbf810000u;

bool writes(std::initializer_list<std::uint32_t> words) {
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{};
    snapshot.code.assign(words.begin(), words.end());
    return AgcDriver::DriverDetail::ProgramWritesMemory(snapshot, 0);
}

void expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("program memory writes: " + what);
}

}

int main() {
    try {
        expect(!writes({0xf800000fu, 0x00000000u, EndProgram}), "an export-only program writes memory");
        expect(!writes({0xe0300000u, 0x80000100u, EndProgram}), "a buffer load writes memory");
        expect(writes({0xe0700000u, 0x80000100u, EndProgram}), "a buffer store does not write memory");
        expect(writes({0xf0200f00u, 0x00000000u, EndProgram}), "an image store does not write memory");
        expect(writes({0xd8360000u, 0x00000000u, EndProgram}), "a GDS write does not write memory");
        expect(writes({0xbe802000u, EndProgram}), "an indirect jump is treated as reading only");
        expect(writes({0x7e000280u}), "code that never ends decodes as reading only");
        AgcDriver::DriverDetail::ShaderSnapshot store{};
        store.code = {0xe0700000u, 0x80000100u, EndProgram};
        expect(AgcDriver::DriverDetail::ProgramWritesMemory(store, 0) && AgcDriver::DriverDetail::ProgramWritesMemory(store, 0), "the cached answer differs");
        expect(!AgcDriver::DriverDetail::ProgramWritesMemory(store, 2), "the program from an offset past the store writes memory");
        LibcRunShutdown_nid_postfix();
        std::puts("program memory writes tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
