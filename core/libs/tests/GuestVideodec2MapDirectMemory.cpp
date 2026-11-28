#include "prx/libSceVideodec2/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace SceVideodec2;

static_assert(sizeof(DirectMemoryInfo) == 32);

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    try {
        int decoder = 0;
        int address = 0;
        DirectMemoryInfo memory{sizeof(DirectMemoryInfo), 0x1000, &address, 0};
        Require(sceVideodec2MapDirectMemory(nullptr, &memory) == static_cast<int>(0x811D0103u), "null decoder must return the decoder instance error");
        Require(sceVideodec2MapDirectMemory(&decoder, nullptr) == static_cast<int>(0x811D0102u), "null memory must return the argument pointer error");
        errno = 71;
        Require(sceVideodec2MapDirectMemory(&decoder, reinterpret_cast<const DirectMemoryInfo*>(0x10)) == static_cast<int>(0x811D0102u), "unreadable memory must return the argument pointer error");
        Require(errno == 71, "copy failure must preserve errno");
        memory.thisSize = sizeof(DirectMemoryInfo) - 1;
        Require(sceVideodec2MapDirectMemory(&decoder, &memory) == static_cast<int>(0x811D0101u), "wrong struct size must return the structure size error");
        memory.thisSize = sizeof(DirectMemoryInfo);
        memory.memoryAddr = nullptr;
        Require(sceVideodec2MapDirectMemory(&decoder, &memory) == static_cast<int>(0x811D0104u), "null memory address must return the memory size error");
        memory.memoryAddr = &address;
        memory.memorySize = 0;
        Require(sceVideodec2MapDirectMemory(&decoder, &memory) == static_cast<int>(0x811D0104u), "zero memory size must return the memory size error");
        memory.memorySize = 0x1000;
        errno = 71;
        bool threw = false;
        try {
            sceVideodec2MapDirectMemory(&decoder, &memory);
        } catch (const std::runtime_error& error) {
            threw = std::string(error.what()) == "sceVideodec2MapDirectMemory not implemented";
        }
        Require(threw, "the accepted path must still report that it is not implemented");
        Require(errno == 71, "the accepted path must preserve errno");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
