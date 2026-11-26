#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "H264Fixture.hpp"
#include "HevcFixture.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace SceVideodec2;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static auto Fields(const DecoderMemoryInfo& memory) {
    return std::tie(memory.thisSize, memory.cpuMemorySize, memory.cpuMemory, memory.gpuMemorySize, memory.gpuMemory,
        memory.cpuGpuMemorySize, memory.cpuGpuMemory, memory.maxFrameBufferSize, memory.frameBufferAlignment);
}

template <typename TAction>
static std::string Failure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    throw std::runtime_error("invalid argument did not throw");
}

int main() {
    try {
        const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, 974921, 1, 0, H264Fixture::Width, H264Fixture::Height, 4, 1};
        DecoderMemoryInfo generic{sizeof(DecoderMemoryInfo)}, alias{sizeof(DecoderMemoryInfo)};
        const auto expected = sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &generic);
        Require(sceVideodec2QueryHevcDecoderMemoryInfo(&config, &alias) == expected && expected == 0, "memory query result differs");
        Require(Fields(alias) == Fields(generic), "memory query fields differ");
        Require(Failure([&] { sceVideodec2QueryHevcDecoderMemoryInfo(nullptr, &alias); }) ==
            Failure([&] { sceVideodec2QueryDecoderMemoryInfo_nid_postfix(nullptr, &generic); }), "query argument error differs");
        std::uint64_t handle = 0, reference = 0;
        Require(Failure([&] { sceVideodec2CreateHevcDecoder(&config, nullptr, &handle); }) ==
            Failure([&] { sceVideodec2CreateDecoder_nid_postfix(&config, nullptr, &reference); }), "create argument error differs");
        Require(sceVideodec2CreateHevcDecoder(&config, &alias, &handle) == 0 && handle != 0, "alias creation failed");
        Require(sceVideodec2CreateDecoder_nid_postfix(&config, &generic, &reference) == 0, "generic creation failed");
        std::vector<std::uint8_t> storage;
        const auto buffer = HevcFixture::AlignedFrameStorage(storage, alias.maxFrameBufferSize, alias.frameBufferAlignment);
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        const InputData empty{sizeof(InputData), nullptr, 0, 0, 0, 0};
        const auto error = sceVideodec2Decode_nid_postfix(reference, &empty, &frame, &output);
        Require(error != 0 && sceVideodec2Decode_nid_postfix(handle, &empty, &frame, &output) == error, "decoder error code differs");
        errno = 71;
        Require(Failure([&] { sceVideodec2Decode_nid_postfix(handle, reinterpret_cast<const InputData*>(0x10), &frame, &output); }) == "Videodec2: invalid argument pointer", "unreadable input pointer did not throw");
        Require(errno == 71, "unreadable input pointer mutated errno");
        errno = 71;
        Require(Failure([&] { sceVideodec2Decode_nid_postfix(handle, &empty, &frame, reinterpret_cast<OutputInfo*>(0x10)); }) == "Videodec2: invalid argument pointer", "unwritable output pointer did not throw");
        Require(errno == 71, "unwritable output pointer mutated errno");
        output.isValid = true;
        output.isErrorFrame = true;
        output.pictureCount = 9;
        output.codecType = 0x12345678u;
        errno = 71;
        Require(Failure([&] { sceVideodec2Decode_nid_postfix(handle, &empty, reinterpret_cast<FrameBuffer*>(0x10), &output); }) == "Videodec2: invalid argument pointer", "unwritable frame pointer did not throw");
        Require(errno == 71, "unwritable frame pointer mutated errno");
        Require(output.isValid && output.isErrorFrame && output.pictureCount == 9 && output.codecType == 0x12345678u, "unwritable frame pointer mutated output");
        if (GuestArena::GuestArenaAvailable_nid_postfix()) {
            constexpr std::size_t kBlock = 0x10000;
            constexpr std::size_t kSpan = 2 * kBlock;
            auto* const mapped = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(kSpan, kBlock));
            Require(mapped != nullptr, "allocate videodec2 guest arena");
#ifdef _WIN32
            const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, kSpan, nullptr);
            Require(section != nullptr, "create videodec2 section");
            GuestArena::GuestArenaMap_nid_postfix(mapped, kSpan, section, 0, PAGE_READWRITE);
            CloseHandle(section);
            const bool watched = GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
            GuestArena::GuestArenaCommit_nid_postfix(mapped, kSpan, 0x04, kBlock);
            const bool watched = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
            if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(mapped, kSpan);
#endif
            auto* const watchedOutput = reinterpret_cast<OutputInfo*>(mapped);
            auto* const watchedFrame = reinterpret_cast<FrameBuffer*>(mapped + kBlock);
            *watchedOutput = OutputInfo{sizeof(OutputInfo), true, true, 9, false, 0x12345678u};
            *watchedFrame = FrameBuffer{sizeof(FrameBuffer), buffer.data(), buffer.size(), true};
            const auto collectDirty = [&]() -> bool {
                if (!watched) return true;
#ifdef _WIN32
                bool dirty = false;
                for (;;) {
                    std::array<void*, 64> pages{};
                    std::size_t count = pages.size();
                    Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kSpan, pages.data(), &count, true), "collect videodec2 writes");
                    if (count != 0) dirty = true;
                    if (count < pages.size()) break;
                }
                return dirty;
#else
                bool dirty = false;
                Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), kSpan,
                    [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &dirty), "collect videodec2 writes");
                return dirty;
#endif
            };
            if (watched) {
                Require(collectDirty(), "initial videodec2 watch flush");
                Require(!collectDirty(), "videodec2 watch clear");
#ifdef _WIN32
                MEMORY_BASIC_INFORMATION mbi{};
                Require(VirtualQuery(watchedOutput, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.Protect == PAGE_READONLY, "arm videodec2 watched output page");
                Require(VirtualQuery(watchedFrame, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.Protect == PAGE_READONLY, "arm videodec2 watched frame page");
#endif
            }
            errno = 71;
            Require(sceVideodec2Decode_nid_postfix(handle, &empty, watchedFrame, watchedOutput) == error, "empty decode on watched output failed");
            Require(errno == 71, "watched empty decode mutated errno");
            Require(!watchedOutput->isValid && !watchedOutput->isErrorFrame && watchedOutput->pictureCount == 0 &&
                watchedOutput->codecType == config.codecType && !watchedFrame->isAccepted, "watched empty decode status mismatch");
            Require(collectDirty(), "watched empty decode did not record dirty page");

            {
                const GuestArena::HostWrite access(mapped, kSpan);
                Require(access.Open(), "prepare watched output for read-only frame check");
                *watchedOutput = OutputInfo{sizeof(OutputInfo), true, true, 9, false, 0x12345678u};
                watchedFrame->isAccepted = true;
            }
            GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(watchedFrame), kBlock, 0x02);
            if (watched) {
                static_cast<void>(collectDirty());
                Require(!collectDirty(), "arm videodec2 watched output before read-only frame check");
            }
            errno = 71;
            Require(Failure([&] { sceVideodec2Decode_nid_postfix(handle, &empty, watchedFrame, watchedOutput); }) == "Videodec2: invalid argument pointer", "read-only frame pointer did not throw");
            Require(errno == 71, "read-only frame pointer mutated errno");
            Require(watchedOutput->isValid && watchedOutput->isErrorFrame && watchedOutput->pictureCount == 9 &&
                watchedOutput->codecType == 0x12345678u && watchedFrame->isAccepted, "read-only frame mutated output before rejecting");
            if (watched) {
                Require(!collectDirty(), "read-only frame dirtied watched output page");
            }
            errno = 71;
            Require(Failure([&] { sceVideodec2Flush_nid_postfix(handle, watchedFrame, watchedOutput); }) == "Videodec2: invalid argument pointer", "read-only flush frame pointer did not throw");
            Require(errno == 71, "read-only flush frame pointer mutated errno");
            Require(watchedOutput->isValid && watchedOutput->isErrorFrame && watchedOutput->pictureCount == 9 &&
                watchedOutput->codecType == 0x12345678u && watchedFrame->isAccepted, "read-only flush frame mutated output");
            if (watched) {
                Require(!collectDirty(), "read-only flush frame dirtied watched output page");
            }
            auto* const straddlingOutput = reinterpret_cast<OutputInfo*>(mapped + kBlock - offsetof(OutputInfo, codecType));
            auto* const straddlingRead = static_cast<unsigned char*>(mapped + kBlock - offsetof(OutputInfo, codecType));
            {
                const GuestArena::HostWrite access(straddlingRead, offsetof(OutputInfo, codecType));
                Require(access.Open(), "prepare straddling videodec2 output prefix");
                const OutputInfo partial{sizeof(OutputInfo), true, true, 9, false, 0u};
                std::memcpy(straddlingRead, &partial, offsetof(OutputInfo, codecType));
            }
            if (watched) {
                static_cast<void>(collectDirty());
                Require(!collectDirty(), "arm videodec2 straddling output");
            }
            errno = 71;
            Require(Failure([&] { sceVideodec2Decode_nid_postfix(handle, &empty, nullptr, straddlingOutput); }) == "Videodec2: invalid argument pointer", "straddling unwritable output did not throw");
            Require(errno == 71, "straddling unwritable output mutated errno");
            Require(straddlingOutput->isValid && straddlingOutput->isErrorFrame && straddlingOutput->pictureCount == 9, "straddling unwritable output mutated writable prefix");
            if (watched) {
                Require(!collectDirty(), "straddling unwritable output dirtied watched prefix page");
            }
            GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(watchedFrame), kBlock, 0x04);
#ifndef _WIN32
            if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(mapped, kSpan);
#endif
            GuestArena::GuestArenaReset_nid_postfix(mapped, kSpan);
            GuestArena::GuestArenaRelease_nid_postfix(mapped, kSpan);
        }
        Require(sceVideodec2DeleteDecoder_nid_postfix(reference) == 0, "generic deletion failed");
        std::size_t pictures = 0;
        const auto take = [&] {
            Require(frame.isAccepted == output.isValid, "frame acceptance differs");
            if (!output.isValid) return false;
            Require(pictures < HevcFixture::HevcHashes.size(), "too many pictures");
            Require(output.codecType == config.codecType && !output.isErrorFrame && output.frameWidth == H264Fixture::Width &&
                output.frameHeight == H264Fixture::Height && output.framePitch == 256 && output.frameBuffer == buffer.data(), "picture geometry differs");
            Require(H264Fixture::hashNv12(buffer.data(), output.framePitch, output.frameHeight) == HevcFixture::HevcHashes[pictures], "picture hash differs");
            ++pictures;
            return true;
        };
        const auto units = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
        for (const auto& unit : units) {
            const InputData input{sizeof(InputData), unit.data(), unit.size(), 0, 0, 0};
            Require(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
            take();
        }
        do {
            Require(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        } while (take());
        Require(pictures == HevcFixture::HevcHashes.size(), "missing pictures");
        Require(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "alias deletion failed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
