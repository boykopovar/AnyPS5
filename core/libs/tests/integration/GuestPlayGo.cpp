#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>

extern "C" {
int APS5_VABI scePlayGoInitialize(const PlayGoInitParams*);
int APS5_VABI scePlayGoOpen(int*, const void*);
int APS5_VABI scePlayGoClose(int);
int APS5_VABI scePlayGoGetLanguageMask(int, std::uint64_t*);
int APS5_VABI scePlayGoSetToDoList(int, const PlayGoToDo*, std::uint32_t);
int APS5_VABI scePlayGoGetToDoList(int, PlayGoToDo*, std::uint32_t, std::uint32_t*);
int APS5_VABI scePlayGoGetLocus(int, const std::uint16_t*, std::uint32_t, std::int8_t*);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int badHandle = static_cast<int>(0x80B20009);
constexpr int badPointer = static_cast<int>(0x80B2000A);
constexpr int badSize = static_cast<int>(0x80B2000B);
constexpr int badChunkId = static_cast<int>(0x80B2000C);
constexpr int badLocus = static_cast<int>(0x80B20010);

class PlayGoSession {
public:
    PlayGoSession() {
        const PlayGoInitParams init{};
        RequireEqual(scePlayGoInitialize(&init), 0, "initialize");
        RequireEqual(scePlayGoOpen(&handle, nullptr), 0, "open");
    }

    ~PlayGoSession() {
        if (!closed) scePlayGoClose(handle);
    }

    PlayGoSession(const PlayGoSession&) = delete;
    PlayGoSession& operator=(const PlayGoSession&) = delete;

    int Close() {
        closed = true;
        return scePlayGoClose(handle);
    }

    int handle = 0;

private:
    bool closed = false;
};

const Case initializeNull{"Initialize_NullParams_ReturnsBadPointer", [] {
    RequireEqual(scePlayGoInitialize(nullptr), badPointer, "initialize with null params");
}};

const Case openNull{"Open_NullHandle_ReturnsBadPointer", [] {
    const PlayGoInitParams init{};
    RequireEqual(scePlayGoInitialize(&init), 0, "initialize");
    RequireEqual(scePlayGoOpen(nullptr, nullptr), badPointer, "open with a null handle");
}};

const Case languageMaskBadHandle{"GetLanguageMask_UnknownHandle_ReturnsBadHandle", [] {
    const PlayGoSession session;
    std::uint64_t mask = 0;
    RequireEqual(scePlayGoGetLanguageMask(session.handle + 1, &mask), badHandle, "language mask of an unknown handle");
}};

const Case languageMaskNull{"GetLanguageMask_NullOutput_ReturnsBadPointer", [] {
    const PlayGoSession session;
    RequireEqual(scePlayGoGetLanguageMask(session.handle, nullptr), badPointer, "language mask with a null output");
}};

const Case languageMask{"GetLanguageMask_OpenHandle_ReportsAllLanguages", [] {
    const PlayGoSession session;
    std::uint64_t mask = 0;
    RequireEqual(scePlayGoGetLanguageMask(session.handle, &mask), 0, "language mask");
    RequireEqual(mask, ~0ull, "language mask value");
}};

const Case setToDoInvalidCalls{"SetToDoList_InvalidHandlePointerOrSize_ReturnsMatchingError", [] {
    const PlayGoSession session;
    const PlayGoToDo todo[2] = {{0, 3, 0}, {0, 0, 0}};
    RequireEqual(scePlayGoSetToDoList(session.handle + 1, todo, 2), badHandle, "unknown handle");
    RequireEqual(scePlayGoSetToDoList(session.handle, nullptr, 2), badPointer, "null list");
    RequireEqual(scePlayGoSetToDoList(session.handle, todo, 0), badSize, "empty list");
}};

const Case setToDoValid{"SetToDoList_ValidEntries_Succeeds", [] {
    const PlayGoSession session;
    const PlayGoToDo todo[2] = {{0, 3, 0}, {0, 0, 0}};
    RequireEqual(scePlayGoSetToDoList(session.handle, todo, 2), 0, "valid list");
}};

const Case setToDoBadLocus{"SetToDoList_UnsupportedLocus_ReturnsBadLocus", [] {
    const PlayGoSession session;
    const PlayGoToDo todo[2] = {{0, 3, 0}, {0, 1, 0}};
    RequireEqual(scePlayGoSetToDoList(session.handle, todo, 2), badLocus, "locus 1");
}};

const Case setToDoBadChunk{"SetToDoList_UnknownChunk_ReturnsBadChunkId", [] {
    const PlayGoSession session;
    const PlayGoToDo todo[2] = {{0, 3, 0}, {0xFFFF, 3, 0}};
    RequireEqual(scePlayGoSetToDoList(session.handle, todo, 2), badChunkId, "chunk 0xFFFF");
}};

const Case getToDo{"GetToDoList_FullyInstalled_ReportsNoEntries", [] {
    const PlayGoSession session;
    PlayGoToDo todo[2] = {};
    std::uint32_t entries = 1;
    RequireEqual(scePlayGoGetToDoList(session.handle, todo, 2, &entries), 0, "get to-do list");
    RequireEqual(entries, 0u, "to-do entries");
}};

const Case locusEmpty{"GetLocus_ZeroEntries_ReturnsBadSize", [] {
    const PlayGoSession session;
    const std::uint16_t chunk = 0;
    std::int8_t locus = 0;
    RequireEqual(scePlayGoGetLocus(session.handle, &chunk, 0, &locus), badSize, "locus of zero chunks");
}};

const Case locus{"GetLocus_ChunkZero_ReportsLocalFast", [] {
    const PlayGoSession session;
    const std::uint16_t chunk = 0;
    std::int8_t locus = 0;
    RequireEqual(scePlayGoGetLocus(session.handle, &chunk, 1, &locus), 0, "locus of chunk 0");
    RequireEqual(locus, std::int8_t{3}, "chunk 0 locus");
}};

const Case close{"Close_UnknownHandleThenOpenHandle_ReturnsBadHandleThenSucceeds", [] {
    PlayGoSession session;
    RequireEqual(scePlayGoClose(session.handle + 1), badHandle, "close of an unknown handle");
    RequireEqual(session.Close(), 0, "close of the open handle");
}};

} // namespace
