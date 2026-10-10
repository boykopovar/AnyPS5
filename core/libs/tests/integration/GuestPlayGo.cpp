#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePlayGoInitialize(const PlayGoInitParams*);
int APS5_VABI scePlayGoOpen(int*, const void*);
int APS5_VABI scePlayGoClose(int);
int APS5_VABI scePlayGoGetLanguageMask(int, std::uint64_t*);
int APS5_VABI scePlayGoSetToDoList(int, const PlayGoToDo*, std::uint32_t);
int APS5_VABI scePlayGoGetToDoList(int, PlayGoToDo*, std::uint32_t, std::uint32_t*);
int APS5_VABI scePlayGoGetLocus(int, const std::uint16_t*, std::uint32_t, std::int8_t*);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int badHandle = static_cast<int>(0x80B20009);
    constexpr int badPointer = static_cast<int>(0x80B2000A);
    constexpr int badSize = static_cast<int>(0x80B2000B);
    constexpr int badChunkId = static_cast<int>(0x80B2000C);
    constexpr int badLocus = static_cast<int>(0x80B20010);

    PlayGoInitParams init{};
    int handle = 0;
    Require(scePlayGoInitialize(nullptr) == badPointer);
    Require(scePlayGoInitialize(&init) == 0);
    Require(scePlayGoOpen(nullptr, nullptr) == badPointer);
    Require(scePlayGoOpen(&handle, nullptr) == 0);

    std::uint64_t mask = 0;
    Require(scePlayGoGetLanguageMask(handle + 1, &mask) == badHandle);
    Require(scePlayGoGetLanguageMask(handle, nullptr) == badPointer);
    Require(scePlayGoGetLanguageMask(handle, &mask) == 0 && mask == ~0ull);

    PlayGoToDo todo[2] = {{0, 3, 0}, {0, 0, 0}};
    Require(scePlayGoSetToDoList(handle + 1, todo, 2) == badHandle);
    Require(scePlayGoSetToDoList(handle, nullptr, 2) == badPointer);
    Require(scePlayGoSetToDoList(handle, todo, 0) == badSize);
    Require(scePlayGoSetToDoList(handle, todo, 2) == 0);
    todo[1].locus = 1;
    Require(scePlayGoSetToDoList(handle, todo, 2) == badLocus);
    todo[1] = {0xFFFF, 3, 0};
    Require(scePlayGoSetToDoList(handle, todo, 2) == badChunkId);

    std::uint32_t entries = 1;
    Require(scePlayGoGetToDoList(handle, todo, 2, &entries) == 0 && entries == 0);
    const std::uint16_t chunk = 0;
    std::int8_t locus = 0;
    Require(scePlayGoGetLocus(handle, &chunk, 0, &locus) == badSize);
    Require(scePlayGoGetLocus(handle, &chunk, 1, &locus) == 0 && locus == 3);
    Require(scePlayGoClose(handle + 1) == badHandle);
    Require(scePlayGoClose(handle) == 0);
}
