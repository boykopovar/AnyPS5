#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JXR list stream check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
Function Exported(Function linked, const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("libSceJxr.prx"), name));
    Require(symbol != nullptr);
    (void)linked;
    return reinterpret_cast<Function>(symbol);
#else
    (void)name;
    return linked;
#endif
}

int liveBlocks = 0;
int zeroedCallsBeforeFailure = -1;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (zeroedCallsBeforeFailure == 0) {
        zeroedCallsBeforeFailure = -1;
        return nullptr;
    }
    if (zeroedCallsBeforeFailure > 0) --zeroedCallsBeforeFailure;
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

std::size_t Offset(Jxr::Stream* stream) { return stream->state[2] + 4096 * stream->state[3]; }

std::uint8_t* FirstLink(Jxr::Stream* stream) { return reinterpret_cast<std::uint8_t*>(stream + 1); }

std::uint8_t* Next(const std::uint8_t* link) {
    std::uint8_t* next;
    std::memcpy(&next, link, sizeof(next));
    return next;
}

Jxr::Stream* MakeList() {
    auto* block = static_cast<std::uint8_t*>(calloc_nid_postfix(1, sizeof(Jxr::Stream) + 4096 + sizeof(void*)));
    Require(block != nullptr);
    auto* stream = reinterpret_cast<Jxr::Stream*>(block);
    stream->state = {reinterpret_cast<std::uintptr_t>(block + sizeof(Jxr::Stream) + sizeof(void*)), 4096, 0, 0};
    return stream;
}

void FreeList(Jxr::Stream* stream) {
    for (auto* link = Next(FirstLink(stream)); link;) {
        auto* next = Next(link);
        free_nid_postfix(link);
        link = next;
    }
    free_nid_postfix(stream);
}

void Fill(Jxr::Stream* stream, const std::vector<std::uint8_t>& data) {
    for (std::size_t done = 0; done < data.size(); done += 4096) {
        const std::size_t size = data.size() - done < 4096 ? data.size() - done : 4096;
        Require(WriteWS_List_nid_postfix(stream, data.data() + done, size) == 0);
    }
}

std::vector<std::uint8_t> Pattern(std::size_t size) {
    std::vector<std::uint8_t> data(size);
    for (std::size_t i = 0; i < size; ++i) data[i] = static_cast<std::uint8_t>(i * 7 + i / 251);
    return data;
}

}

int main() {
    InitializeHeap();
    {
        auto* stream = MakeList();
        const auto data = Pattern(9000);
        auto* first = reinterpret_cast<std::uint8_t*>(stream->state[0]);
        Require(WriteWS_List_nid_postfix(stream, data.data(), 0) == 0 && Offset(stream) == 0);
        Require(WriteWS_List_nid_postfix(stream, data.data(), 100) == 0 && stream->state[2] == 100 && liveBlocks == 1);
        Require(WriteWS_List_nid_postfix(stream, data.data() + 100, 3996) == 0 && liveBlocks == 2);
        auto* second = Next(FirstLink(stream));
        Require(second && stream->state[0] == reinterpret_cast<std::uintptr_t>(second + sizeof(void*)) && !Next(second));
        Require(stream->state[1] == 8192 && stream->state[2] == 0 && stream->state[3] == 1);
        Require(std::memcmp(first, data.data(), 4096) == 0);
        Require(WriteWS_List_nid_postfix(stream, data.data(), 8193) == -103 && Offset(stream) == 4096 && liveBlocks == 2);
        Require(WriteWS_List_nid_postfix(stream, data.data(), SIZE_MAX) == -103);
        Require(WriteWS_List_nid_postfix(stream, data.data() + 4096, 4000) == 0 && stream->state[2] == 4000);
        Require(std::memcmp(second + sizeof(void*), data.data() + 4096, 4000) == 0);
        zeroedCallsBeforeFailure = 0;
        Require(WriteWS_List_nid_postfix(stream, data.data() + 8096, 96) == -101 && stream->state[2] == 4096);
        Require(stream->state[1] == 8192 && stream->state[3] == 1 && liveBlocks == 2 && !Next(second));
        Require(WriteWS_List_nid_postfix(stream, data.data() + 8192, 8) == 0 && liveBlocks == 3);
        Require(stream->state[1] == 12288 && stream->state[2] == 8 && stream->state[3] == 2);
        FreeList(stream);
        Require(liveBlocks == 0);
    }
    {
        auto* stream = MakeList();
        const auto data = Pattern(5000);
        Fill(stream, data);
        Require(liveBlocks == 2);
        auto* first = FirstLink(stream) + sizeof(void*);
        auto* second = Next(FirstLink(stream));
        stream->state[0] = reinterpret_cast<std::uintptr_t>(first);
        stream->state[2] = 0;
        stream->state[3] = 0;
        std::vector<std::uint8_t> read(8192, 0xcc);
        Require(ReadWS_List_nid_postfix(stream, read.data(), 3000) == 0 && Offset(stream) == 3000);
        Require(ReadWS_List_nid_postfix(stream, read.data() + 3000, 2000) == 0 && Offset(stream) == 5000);
        Require(stream->state[0] == reinterpret_cast<std::uintptr_t>(second + sizeof(void*)) && stream->state[3] == 1);
        Require(std::memcmp(read.data(), data.data(), 5000) == 0);
        Require(ReadWS_List_nid_postfix(stream, read.data() + 5000, 3192) == 0);
        Require(stream->state[0] == sizeof(void*) && stream->state[2] == 0 && stream->state[3] == 2);
        Require(std::all_of(read.begin() + 5000, read.end(), [](std::uint8_t value) { return value == 0; }));
        read.assign(16, 0xcc);
        Require(ReadWS_List_nid_postfix(stream, read.data(), 16) == 0 && read[0] == 0xcc && Offset(stream) == 8192);
        stream->state[2] = 1;
        Require(ReadWS_List_nid_postfix(stream, read.data(), SIZE_MAX) == -103 && stream->state[2] == 1);
        stream->state = {reinterpret_cast<std::uintptr_t>(second + sizeof(void*)), 8192, 900, 1};
        Require(ReadWS_List_nid_postfix(stream, read.data(), 16) == 0 && Offset(stream) == 5012);
        Require(std::memcmp(read.data(), data.data() + 4996, 4) == 0 && read[4] == 0);
        FreeList(stream);
        Require(liveBlocks == 0);
    }
    {
        auto* stream = MakeList();
        const auto data = Pattern(9000);
        Fill(stream, data);
        Require(liveBlocks == 3);
        auto* second = Next(FirstLink(stream));
        auto* third = Next(second);
        Require(SetPosWS_List_nid_postfix(stream, 5000) == 0 && Offset(stream) == 5000 && stream->state[3] == 1);
        Require(stream->state[0] == reinterpret_cast<std::uintptr_t>(second + sizeof(void*)));
        std::uint8_t byte = 0;
        Require(ReadWS_List_nid_postfix(stream, &byte, 1) == 0 && byte == data[5000]);
        Require(SetPosWS_List_nid_postfix(stream, 0) == 0 && Offset(stream) == 0);
        Require(stream->state[0] == reinterpret_cast<std::uintptr_t>(FirstLink(stream) + sizeof(void*)));
        Require(SetPosWS_List_nid_postfix(stream, 8192) == 0 && stream->state[2] == 0 && stream->state[3] == 2);
        Require(stream->state[0] == reinterpret_cast<std::uintptr_t>(third + sizeof(void*)));
        Require(SetPosWS_List_nid_postfix(stream, 20000) == 0 && stream->state[2] == 0 && stream->state[3] == 3);
        Require(stream->state[0] == reinterpret_cast<std::uintptr_t>(third + sizeof(void*)));
        Require(SetPosWS_List_nid_postfix(stream, 12287) == 0 && stream->state[2] == 4095 && stream->state[3] == 2);
        FreeList(stream);
        Require(liveBlocks == 0);
    }
    {
        auto* stream = MakeList();
        const auto data = Pattern(5000);
        Fill(stream, data);
        std::size_t position = 1;
        Require(GetPosWS_List_nid_postfix(stream, &position) == 0 && position == 5000);
        Require(SetPosWS_List_nid_postfix(stream, 4097) == 0 && GetPosWS_List_nid_postfix(stream, &position) == 0 && position == 4097);
        Require(SetPosWS_List_nid_postfix(stream, 9000) == 0 && GetPosWS_List_nid_postfix(stream, &position) == 0 && position == 8192);
        FreeList(stream);
        Require(liveBlocks == 0);
    }
    {
        auto* stream = MakeList();
        const auto data = Pattern(13000);
        Fill(stream, data);
        Require(liveBlocks == 4);
        Require(CloseWS_List_nid_postfix(&stream) == 0 && !stream && liveBlocks == 0);
        stream = MakeList();
        Require(CloseWS_List_nid_postfix(&stream) == 0 && !stream && liveBlocks == 0);
    }
    {
        Jxr::Stream* stream = reinterpret_cast<Jxr::Stream*>(1);
        zeroedCallsBeforeFailure = 0;
        Require(CreateWS_List_nid_postfix(&stream) == -101 && !stream && liveBlocks == 0);
        Require(CreateWS_List_nid_postfix(&stream) == 0 && stream && liveBlocks == 1);
        Require(stream->state[0] == reinterpret_cast<std::uintptr_t>(FirstLink(stream) + sizeof(void*)) && !Next(FirstLink(stream)));
        Require(stream->state[1] == 4096 && stream->state[2] == 0 && stream->state[3] == 0 && stream->memory == 0);
        Require(stream->close == Exported(&CloseWS_List_nid_postfix, "CloseWS_List_nid_postfix") && !stream->end && stream->read == Exported(&ReadWS_List_nid_postfix, "ReadWS_List_nid_postfix"));
        Require(stream->write == Exported(&WriteWS_List_nid_postfix, "WriteWS_List_nid_postfix") && stream->setPosition == Exported(&SetPosWS_List_nid_postfix, "SetPosWS_List_nid_postfix"));
        Require(stream->getPosition == Exported(&GetPosWS_List_nid_postfix, "GetPosWS_List_nid_postfix"));
        const auto data = Pattern(10000);
        Require(stream->write(stream, data.data(), 4096) == 0 && stream->write(stream, data.data() + 4096, 4096) == 0);
        Require(stream->write(stream, data.data() + 8192, 1808) == 0);
        std::size_t position = 0;
        Require(stream->getPosition(stream, &position) == 0 && position == 10000 && liveBlocks == 3);
        std::vector<std::uint8_t> read(10000);
        Require(stream->setPosition(stream, 0) == 0 && stream->read(stream, read.data(), 10000) == 0 && read == data);
        Require(stream->close(&stream) == 0 && !stream && liveBlocks == 0);
    }
}
