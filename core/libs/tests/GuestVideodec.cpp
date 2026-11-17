#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libSceVideodec/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "H264Fixture.hpp"
#include "Testing.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace SceVideodec;

static_assert(sizeof(PictureInfo) == 112 && sizeof(AvcInfo) == 36);
static_assert(sizeof(ConfigInfo) == 40 && sizeof(ResourceInfo) == 56 && sizeof(Ctrl) == 24);
static_assert(sizeof(SceVideodec2::DecoderConfigInfo) == 40 && sizeof(SceVideodec2::DecoderMemoryInfo) == 72);

namespace {

using namespace SceVideodec;

using namespace H264Fixture;

constexpr int ErrorApiFail = static_cast<int>(0x80C10000);
constexpr int ErrorStructSize = static_cast<int>(0x80C10002);
constexpr int ErrorHandle = static_cast<int>(0x80C10003);
constexpr int ErrorAuSize = static_cast<int>(0x80C10009);
constexpr int ErrorAuPointer = static_cast<int>(0x80C1000A);
constexpr int ErrorFrameBufferSize = static_cast<int>(0x80C1000B);
constexpr int ErrorFrameBufferPointer = static_cast<int>(0x80C1000C);
constexpr int ErrorConfigInfo = static_cast<int>(0x80C1000E);
constexpr int ErrorArgumentPointer = static_cast<int>(0x80C1000F);

constexpr std::uint32_t Pitch = 64;
constexpr std::uint32_t PictureHeight = 48;
constexpr std::size_t FrameBytes = static_cast<std::size_t>(Pitch) * PictureHeight * 3 / 2;

class PageTail {
    void* memory = nullptr;
    std::size_t page = 0;
    void* data = nullptr;

public:
    PageTail(const void* source, std::size_t bytes) {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(memory != nullptr, "the guard pages could not be allocated");
        DWORD previous;
        Require(VirtualProtect(static_cast<unsigned char*>(memory) + page, page, PAGE_NOACCESS, &previous) != 0, "the guard page could not be protected");
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        memory = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED, "the guard pages could not be allocated");
        Require(mprotect(static_cast<unsigned char*>(memory) + page, page, PROT_NONE) == 0, "the guard page could not be protected");
#endif
        Require(bytes <= page, "the guarded value does not fit in a page");
        data = static_cast<unsigned char*>(memory) + page - bytes;
        std::memcpy(data, source, bytes);
    }
    PageTail(const PageTail&) = delete;
    PageTail& operator=(const PageTail&) = delete;
    ~PageTail() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, page * 2);
#endif
    }
    void ReadOnly() {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != 0, "the page could not be made read-only");
#else
        Require(mprotect(memory, page, PROT_READ) == 0, "the page could not be made read-only");
#endif
    }
    template <typename T>
    T* As() const { return static_cast<T*>(data); }
};

struct Session {
    Ctrl ctrl{sizeof(Ctrl)};
    std::vector<std::uint8_t> buffer;

    Session() : buffer(FrameBytes) {
        const ConfigInfo config{sizeof(ConfigInfo), 1, 100, 42, static_cast<std::int32_t>(Width),
                                static_cast<std::int32_t>(CodedHeight), 4, 0};
        const ResourceInfo resource{sizeof(ResourceInfo), 16u << 20u, nullptr, 16u << 20u, nullptr,
                                    FrameBytes, 256};
        Require(sceVideodecCreateDecoder(&config, &resource, &ctrl) == 0, "decoder creation failed");
        Require(ctrl.handle != nullptr && ctrl.version == 1, "the control block was not filled");
    }

    ~Session() { sceVideodecDeleteDecoder(&ctrl); }

    FrameBuffer MakeTarget() { return FrameBuffer{sizeof(FrameBuffer), buffer.data(), buffer.size()}; }
};

void testDecode(const std::vector<std::vector<std::uint8_t>>& units, std::uint32_t cropTop, std::uint32_t cropBottom) {
    Session session;
    Require(units.size() == PictureHashes.size(), "unexpected access unit count");
    std::vector<std::uint64_t> hashes;
    std::vector<std::uint64_t> timestamps;
    std::vector<std::uint64_t> attachments;
    const auto collect = [&](PictureInfo& picture) {
        if (!picture.isValid) return;
        Require(picture.framePitch == Pitch && picture.frameHeight == PictureHeight, "unexpected picture geometry");
        Require(picture.frameWidth == Width, "unexpected picture width");
        Require(picture.codecType == 0 && !picture.isErrorPic, "unexpected picture flags");
        Require(picture.codec.avc.frameCropTopOffset == cropTop && picture.codec.avc.frameCropBottomOffset == cropBottom, "unexpected crop");
        hashes.push_back(hashNv12(session.buffer.data(), Pitch, PictureHeight));
        timestamps.push_back(picture.ptsData);
        attachments.push_back(picture.attachedData);
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const SceVideodec2::InputData input{sizeof(SceVideodec2::InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        auto target = session.MakeTarget();
        PictureInfo picture{sizeof(PictureInfo)};
        const auto status = sceVideodecDecode(&session.ctrl, &input, &target, &picture);
        Require(status == 0, "decode of unit " + std::to_string(unit) + " failed with " + std::to_string(status));
        collect(picture);
    }
    for (;;) {
        auto target = session.MakeTarget();
        PictureInfo picture{sizeof(PictureInfo)};
        Require(sceVideodecFlush(&session.ctrl, &target, &picture) == 0, "flush failed");
        if (!picture.isValid) break;
        collect(picture);
    }
    Require(hashes.size() == PictureHashes.size(), "missing pictures");
    for (std::size_t index = 0; index < hashes.size(); ++index) {
        Require(hashes[index] == PictureHashes[index], "picture " + std::to_string(index) + " differs from the reference");
        const auto unit = DisplayOrderUnits[index];
        Require(timestamps[index] == 1000 + unit, "pictures out of display order");
        Require(attachments[index] == 0xa0 + unit, "picture attachment out of display order");
    }
}

void testDecode() {
    testDecode(accessUnits(false), 0, (CodedHeight - Height) / 2);
}

void testDecodeCropped() {
    const auto stream = croppedStream();
    testDecode(accessUnits(stream.data(), stream.size(), false), 4, 4);
}

void testErrors() {
    const ConfigInfo config{sizeof(ConfigInfo), 1, 100, 42, static_cast<std::int32_t>(Width),
                            static_cast<std::int32_t>(CodedHeight), 4, 0};
    const ResourceInfo resource{sizeof(ResourceInfo), 16u << 20u, nullptr, 16u << 20u, nullptr, FrameBytes, 256};
    Ctrl ctrl{sizeof(Ctrl)};
    Require(sceVideodecCreateDecoder(nullptr, &resource, &ctrl) == ErrorArgumentPointer, "a null configuration was accepted");
    Require(sceVideodecCreateDecoder(&config, &resource, nullptr) == ErrorArgumentPointer, "a null control block was accepted");
    const ConfigInfo shortConfig{sizeof(ConfigInfo) - 8};
    Require(sceVideodecCreateDecoder(&shortConfig, &resource, &ctrl) == ErrorStructSize, "a short configuration was accepted");
    Require(sceVideodecCreateDecoder(&config, &resource, &ctrl) == 0, "decoder creation failed");
    PictureInfo freshPicture{sizeof(PictureInfo)};
    FrameBuffer unset{sizeof(FrameBuffer), nullptr, FrameBytes};
    Require(sceVideodecFlush(&ctrl, &unset, &freshPicture) == ErrorFrameBufferPointer, "flush accepted a null frame buffer");
    Require(sceVideodecDeleteDecoder(nullptr) == ErrorArgumentPointer, "a null control block was accepted by delete");
    Ctrl shortCtrl{sizeof(Ctrl) - 8};
    Require(sceVideodecDeleteDecoder(&shortCtrl) == ErrorStructSize, "a short control block was accepted by delete");
    Require(sceVideodecReset(nullptr) == ErrorArgumentPointer, "a null control block was accepted by reset");
    Require(sceVideodecReset(&shortCtrl) == ErrorStructSize, "a short control block was accepted by reset");
    Ctrl foreign{sizeof(Ctrl)};
    foreign.handle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234));
    Require(sceVideodecDeleteDecoder(&foreign) == ErrorHandle, "an unknown decoder was deleted");
    Require(sceVideodecReset(&foreign) == ErrorHandle, "an unknown decoder was reset");
    const SceVideodec2::InputData empty{sizeof(SceVideodec2::InputData), nullptr, 0, 0, 0, 0};
    FrameBuffer frame{sizeof(FrameBuffer), nullptr, 0};
    PictureInfo picture{sizeof(PictureInfo)};
    auto* const badCtrl = reinterpret_cast<Ctrl*>(std::uintptr_t{0x10});
    auto* const badInput = reinterpret_cast<const SceVideodec2::InputData*>(std::uintptr_t{0x10});
    auto* const badFrame = reinterpret_cast<FrameBuffer*>(std::uintptr_t{0x10});
    auto* const badPicture = reinterpret_cast<PictureInfo*>(std::uintptr_t{0x10});
    Require(sceVideodecDecode(badCtrl, &empty, &frame, &picture) == ErrorArgumentPointer, "an unreadable control block was accepted by decode");
    Require(sceVideodecDecode(&ctrl, badInput, &frame, &picture) == ErrorArgumentPointer, "an unreadable input block was accepted by decode");
    Require(sceVideodecDecode(&ctrl, &empty, badFrame, &picture) == ErrorFrameBufferPointer, "an unreadable frame buffer was accepted by decode");
    Require(sceVideodecDecode(&ctrl, &empty, &frame, badPicture) == ErrorArgumentPointer, "an unreadable picture info was accepted by decode");
    Require(sceVideodecFlush(badCtrl, &frame, &picture) == ErrorArgumentPointer, "an unreadable control block was accepted by flush");
    Require(sceVideodecFlush(&ctrl, badFrame, &picture) == ErrorFrameBufferPointer, "an unreadable frame buffer was accepted by flush");
    Require(sceVideodecFlush(&ctrl, &frame, badPicture) == ErrorArgumentPointer, "an unreadable picture info was accepted by flush");
    Require(sceVideodecDecode(nullptr, &empty, &frame, &picture) == ErrorArgumentPointer, "a null control block was accepted");
    Require(sceVideodecDecode(&ctrl, &empty, &frame, &picture) == ErrorAuPointer, "a null access unit was accepted");
    const SceVideodec2::InputData sized{sizeof(SceVideodec2::InputData), reinterpret_cast<const std::uint8_t*>(&config), 0, 0, 0, 0};
    Require(sceVideodecDecode(&ctrl, &sized, &frame, &picture) == ErrorAuSize, "an empty access unit was accepted");
    FrameBuffer missing{sizeof(FrameBuffer), nullptr, FrameBytes};
    const auto units = accessUnits(false);
    const SceVideodec2::InputData input{sizeof(SceVideodec2::InputData), units[0].data(), units[0].size(), 0, 0, 0};
    Require(sceVideodecDecode(&ctrl, &input, &missing, &picture) == ErrorFrameBufferPointer, "a null frame buffer was accepted");
    std::vector<std::uint8_t> storage(static_cast<std::size_t>(FrameBytes));
    FrameBuffer shortBuffer{sizeof(FrameBuffer), storage.data(), FrameBytes - 1};
    int refusals = 0;
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const SceVideodec2::InputData unit0{sizeof(SceVideodec2::InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        const auto status = sceVideodecDecode(&ctrl, &unit0, &shortBuffer, &picture);
        Require(status == 0 || status == ErrorFrameBufferSize, "a decode failed");
        if (status == ErrorFrameBufferSize) ++refusals;
    }
    Require(refusals > 0, "no picture was refused for the short frame buffer");
    FrameBuffer shortFrame{sizeof(FrameBuffer) - 8, storage.data(), FrameBytes};
    Require(sceVideodecDecode(&ctrl, &input, &shortFrame, &picture) == ErrorStructSize, "a short frame buffer structure was accepted");
    PictureInfo shortPicture{sizeof(PictureInfo) - 8};
    Require(sceVideodecDecode(&ctrl, &input, &frame, &shortPicture) == ErrorStructSize, "a short picture information structure was accepted");
    Require(sceVideodecDecode(&ctrl, &input, nullptr, nullptr) == ErrorArgumentPointer, "decode reported the frame buffer before a null picture info");
    Require(sceVideodecDecode(&ctrl, nullptr, nullptr, &picture) == ErrorArgumentPointer, "decode reported the frame buffer before a null input block");
    const SceVideodec2::InputData shortInput{sizeof(SceVideodec2::InputData) - 8, units[0].data(), units[0].size(), 0, 0, 0};
    Require(sceVideodecDecode(&ctrl, &shortInput, &frame, &picture) == ErrorStructSize, "a short input structure was accepted");
    Require(sceVideodecFlush(&ctrl, nullptr, nullptr) == ErrorArgumentPointer, "flush reported the frame buffer before a null picture info");
    Require(sceVideodecFlush(&foreign, &frame, &picture) == ErrorHandle, "flush accepted an unknown decoder");
    Require(sceVideodecDeleteDecoder(&ctrl) == 0, "the decoder was not deleted");
    Require(sceVideodecDeleteDecoder(&ctrl) == ErrorHandle, "the decoder was deleted twice");
    Require(sceVideodecReset(&ctrl) == ErrorHandle, "a deleted decoder was reset");
    Require(sceVideodecDecode(&ctrl, &input, &frame, &picture) == ErrorHandle, "a decode succeeded on a deleted decoder");
}

void testReset() {
    Session session;
    const auto units = accessUnits(false);
    const SceVideodec2::InputData input{sizeof(SceVideodec2::InputData), units[0].data(), units[0].size(), 1000, 0, 0xa0};
    auto target = session.MakeTarget();
    PictureInfo picture{sizeof(PictureInfo)};
    Require(sceVideodecDecode(&session.ctrl, &input, &target, &picture) == 0, "decode failed");
    Require(sceVideodecReset(&session.ctrl) == 0, "reset failed");
    Require(sceVideodecFlush(&session.ctrl, &target, &picture) == 0, "flush failed");
    Require(!picture.isValid, "a reset decoder produced a picture");
}

constexpr std::uint8_t High10Picture[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x6e, 0x10, 0x0a, 0xa6, 0xcb, 0x88, 0xf6, 0x02, 0x20, 0x00, 0x00,
    0x03, 0x00, 0x20, 0x00, 0x00, 0x03, 0x00, 0x40, 0x80, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x0f,
    0xc8, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x3a, 0x26, 0x28, 0x0c, 0x9c, 0x9c, 0x9d, 0x75, 0xd7,
    0x5d, 0x75, 0xd7, 0x5e
};

void testDeepPicture() {
    Session session;
    const SceVideodec2::InputData input{sizeof(SceVideodec2::InputData), High10Picture, sizeof(High10Picture), 1000, 0, 0xa0};
    bool refused = false;
    std::size_t pictures = 0;
    try {
        auto target = session.MakeTarget();
        PictureInfo picture{sizeof(PictureInfo)};
        Require(sceVideodecDecode(&session.ctrl, &input, &target, &picture) == 0, "decode of the 10-bit picture failed");
        if (picture.isValid) ++pictures;
        for (;;) {
            Require(sceVideodecFlush(&session.ctrl, &target, &picture) == 0, "flush of the 10-bit picture failed");
            if (!picture.isValid) break;
            ++pictures;
        }
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("deeper than 8 bits") == std::string::npos) throw;
        refused = true;
    }
    Require(refused, "10-bit pictures copied as 8-bit pictures: " + std::to_string(pictures));
}

constexpr std::uint8_t High10ReorderedPicture[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x6e, 0x00, 0x0a, 0xa6, 0xce, 0x41, 0x1e, 0xc0, 0x44, 0x00, 0x00,
    0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xc8, 0x3c, 0x48, 0x94, 0x48, 0x00, 0x00, 0x00, 0x01,
    0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x43, 0xff,
    0xfe, 0xf7, 0x81, 0xbf, 0x32, 0x7d, 0xaa, 0xb2, 0x5c, 0x7e, 0x79, 0xdf, 0x14, 0x3f, 0x28, 0x74,
    0xab, 0x06, 0xa2, 0xac, 0xeb, 0x0d
};

void testDeepReorderedPicture() {
    Session session;
    const SceVideodec2::InputData input{sizeof(SceVideodec2::InputData), High10ReorderedPicture, sizeof(High10ReorderedPicture), 1000, 0, 0xa0};
    auto target = session.MakeTarget();
    PictureInfo picture{sizeof(PictureInfo)};
    Require(sceVideodecDecode(&session.ctrl, &input, &target, &picture) == 0, "decode of the reordered 10-bit picture failed");
    Require(!picture.isValid, "the reordered 10-bit picture was not held for the flush");
    bool refused = false;
    try {
        sceVideodecFlush(&session.ctrl, &target, &picture);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("deeper than 8 bits") == std::string::npos) throw;
        refused = true;
    }
    Require(refused, "a flushed 10-bit picture was copied as an 8-bit picture");
}

void testQueryResourceInfo(std::int32_t width, std::int32_t height, std::uint32_t profile, std::uint32_t level) {
    const ConfigInfo config{sizeof(ConfigInfo), 1, profile, level, width, height, 4, 0};
    ResourceInfo resource{sizeof(ResourceInfo)};
    Require(sceVideodecQueryResourceInfo(&config, &resource) == 0, "the resource query failed");
    const SceVideodec2::DecoderConfigInfo mapped{sizeof(SceVideodec2::DecoderConfigInfo), 1, 1, profile, level, width, height, 4, 1};
    SceVideodec2::DecoderMemoryInfo memory{sizeof(SceVideodec2::DecoderMemoryInfo)};
    Require(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&mapped, &memory) == 0, "the Videodec2 memory query failed");
    Require(resource.thisSize == sizeof(ResourceInfo), "the resource information size was changed");
    Require(resource.cpuMemorySize == memory.cpuMemorySize, "the CPU memory size differs from libSceVideodec2");
    Require(resource.cpuGpuMemorySize == memory.cpuGpuMemorySize, "the CPU-GPU memory size differs from libSceVideodec2");
    Require(resource.maxFrameBufferSize == memory.maxFrameBufferSize, "the frame buffer size differs from libSceVideodec2");
    Require(resource.frameBufferAlignment == memory.frameBufferAlignment, "the frame buffer alignment differs from libSceVideodec2");
}

void testQueryResourceInfo() {
    testQueryResourceInfo(static_cast<std::int32_t>(Width), static_cast<std::int32_t>(CodedHeight), 100, 42);
    testQueryResourceInfo(1920, 1080, 100, 42);
    testQueryResourceInfo(3840, 2160, 100, 51);
    const ConfigInfo config{sizeof(ConfigInfo), 1, 100, 42, static_cast<std::int32_t>(Width),
                            static_cast<std::int32_t>(CodedHeight), 4, 0};
    ResourceInfo resource{sizeof(ResourceInfo)};
    Require(sceVideodecQueryResourceInfo(&config, &resource) == 0, "the resource query failed");
    Require(resource.maxFrameBufferSize >= FrameBytes, "the frame buffer size does not hold a decoded picture");
    Ctrl ctrl{sizeof(Ctrl)};
    Require(sceVideodecCreateDecoder(&config, &resource, &ctrl) == 0, "the queried resources were refused");
    Require(sceVideodecDeleteDecoder(&ctrl) == 0, "the decoder was not deleted");
    auto* const badConfig = reinterpret_cast<const ConfigInfo*>(std::uintptr_t{0x10});
    auto* const badResource = reinterpret_cast<ResourceInfo*>(std::uintptr_t{0x10});
    auto* const badCtrl = reinterpret_cast<Ctrl*>(std::uintptr_t{0x10});
    Require(sceVideodecQueryResourceInfo(nullptr, &resource) == ErrorArgumentPointer, "a null configuration was accepted");
    Require(sceVideodecQueryResourceInfo(&config, nullptr) == ErrorArgumentPointer, "null resource information was accepted");
    Require(sceVideodecQueryResourceInfo(badConfig, &resource) == ErrorArgumentPointer, "an unreadable configuration was accepted");
    Require(sceVideodecQueryResourceInfo(&config, badResource) == ErrorArgumentPointer, "an unwritable resource information pointer was accepted");
    Require(sceVideodecCreateDecoder(badConfig, &resource, &ctrl) == ErrorArgumentPointer, "an unreadable configuration was accepted by create");
    Require(sceVideodecCreateDecoder(&config, badResource, &ctrl) == ErrorArgumentPointer, "an unreadable resource was accepted by create");
    Require(sceVideodecCreateDecoder(&config, &resource, badCtrl) == ErrorArgumentPointer, "an unwritable control block was accepted by create");
    Require(sceVideodecDeleteDecoder(badCtrl) == ErrorArgumentPointer, "an unreadable control block was accepted by delete");
    Require(sceVideodecReset(badCtrl) == ErrorArgumentPointer, "an unreadable control block was accepted by reset");
    const ResourceInfo sentinel{sizeof(ResourceInfo), 7, nullptr, 7, nullptr, 7, 7};
    auto refused = sentinel;
    auto shortConfig = config;
    shortConfig.thisSize = sizeof(ConfigInfo) - 8;
    Require(sceVideodecQueryResourceInfo(&shortConfig, &refused) == ErrorStructSize, "a short configuration was accepted");
    auto shortResource = sentinel;
    shortResource.thisSize = sizeof(ResourceInfo) - 8;
    Require(sceVideodecQueryResourceInfo(&config, &shortResource) == ErrorStructSize, "short resource information was accepted");
    auto unsized = config;
    unsized.maxFrameWidth = 0;
    Require(sceVideodecQueryResourceInfo(&unsized, &refused) == ErrorConfigInfo, "a configuration without a frame size was accepted");
    Require(refused.cpuMemorySize == 7 && refused.cpuGpuMemorySize == 7 && refused.maxFrameBufferSize == 7 && refused.frameBufferAlignment == 7,
          "a refused query wrote the resource information");
    Require(shortResource.cpuMemorySize == 7 && shortResource.maxFrameBufferSize == 7, "a refused query wrote the resource information");

    auto wideConfig = config;
    wideConfig.thisSize = std::uint64_t{1} << 32;
    auto wideResource = resource;
    wideResource.thisSize = std::uint64_t{1} << 32;
    Require(sceVideodecQueryResourceInfo(&wideConfig, &wideResource) == 0, "a size field with only its upper bits set was refused by query");
    Ctrl wideCtrl{std::uint64_t{1} << 32};
    Require(sceVideodecCreateDecoder(&wideConfig, &resource, &wideCtrl) == 0, "a size field with only its upper bits set was refused by create");
    wideCtrl.thisSize = std::uint64_t{1} << 32;
    Require(sceVideodecDeleteDecoder(&wideCtrl) == 0, "a control block size with only its upper bits set was refused by delete");

    Ctrl kept{0x5a5a5a5a5a5a5a5au, reinterpret_cast<void*>(std::uintptr_t{0x5a}), 0x5au};
    auto unsizedConfig = config;
    unsizedConfig.maxFrameWidth = 0;
    Require(sceVideodecCreateDecoder(&unsizedConfig, &resource, &kept) == ErrorConfigInfo, "create accepted a configuration without a frame size");
    Require(kept.thisSize == 0x5a5a5a5a5a5a5a5au && kept.handle == reinterpret_cast<void*>(std::uintptr_t{0x5a}) && kept.version == 0x5au, "a failed create wrote the control block");

    const std::uint64_t shortSize = 8;
    PageTail shortCtrl(&shortSize, sizeof(shortSize));
    PageTail shortFrame(&shortSize, sizeof(shortSize));
    PageTail shortPicture(&shortSize, sizeof(shortSize));
    const SceVideodec2::InputData empty{sizeof(SceVideodec2::InputData), nullptr, 0, 0, 0, 0};
    FrameBuffer frame{sizeof(FrameBuffer), nullptr, 0};
    PictureInfo picture{sizeof(PictureInfo)};
    Ctrl sized{sizeof(Ctrl)};
    Require(sceVideodecDeleteDecoder(shortCtrl.As<Ctrl>()) == ErrorStructSize, "delete gave a pointer error for a short control block at the end of a page");
    Require(sceVideodecReset(shortCtrl.As<Ctrl>()) == ErrorStructSize, "reset gave a pointer error for a short control block at the end of a page");
    Require(sceVideodecFlush(shortCtrl.As<Ctrl>(), &frame, &picture) == ErrorStructSize, "flush gave a pointer error for a short control block at the end of a page");
    Require(sceVideodecFlush(&sized, shortFrame.As<FrameBuffer>(), &picture) == ErrorStructSize, "flush gave a pointer error for a short frame buffer at the end of a page");
    Require(sceVideodecFlush(&sized, &frame, shortPicture.As<PictureInfo>()) == ErrorStructSize, "flush gave a pointer error for a short picture at the end of a page");
    Require(sceVideodecDecode(shortCtrl.As<Ctrl>(), &empty, &frame, &picture) == ErrorStructSize, "decode gave a pointer error for a short control block at the end of a page");
    Require(sceVideodecDecode(&sized, &empty, shortFrame.As<FrameBuffer>(), &picture) == ErrorStructSize, "decode gave a pointer error for a short frame buffer at the end of a page");

    PageTail readOnlyResource(&resource, sizeof(resource));
    readOnlyResource.ReadOnly();
    Require(sceVideodecQueryResourceInfo(&config, readOnlyResource.As<ResourceInfo>()) == ErrorArgumentPointer, "query accepted a read-only resource information block");
    const Ctrl unwritten{sizeof(Ctrl)};
    PageTail readOnlyCtrl(&unwritten, sizeof(unwritten));
    readOnlyCtrl.ReadOnly();
    Require(sceVideodecCreateDecoder(&config, &resource, readOnlyCtrl.As<Ctrl>()) == ErrorArgumentPointer, "create accepted a read-only control block");
    Require(sceVideodecCreateDecoder(&config, &resource, &ctrl) == 0, "create failed after a refused read-only control block");
    Require(sceVideodecDeleteDecoder(&ctrl) == 0, "the decoder created after a refused read-only control block was not deleted");
}

}

int main() {
    try {
        testDecode();
        testDecodeCropped();
        testErrors();
        testReset();
        testDeepPicture();
        testDeepReorderedPicture();
        testQueryResourceInfo();
        std::puts("Videodec tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
