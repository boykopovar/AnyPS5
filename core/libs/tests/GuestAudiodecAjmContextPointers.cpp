#include "prx/libSceAudiodec.native/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) { errno = 99; }

constexpr int InvalidArgument = static_cast<int>(0x807F0002u);
constexpr int InvalidCtrlPointer = static_cast<int>(0x807F0008u);
constexpr int InvalidAuInfoPointer = static_cast<int>(0x807F000Bu);
constexpr int InvalidPcmItemPointer = static_cast<int>(0x807F000Cu);
constexpr int InvalidPcmPointer = static_cast<int>(0x807F000Eu);
constexpr int AlreadyRegistered = static_cast<int>(0x80930009u);
constexpr int NotRegistered = static_cast<int>(0x8093000Au);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

using GetAjmContextFn = int (APS5_VABI*)(const void*, std::uint32_t*, std::uint32_t*);
using UnregisterCodecFn = int (APS5_VABI*)(const void*, std::uint32_t);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceAudiodec.native.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceAudiodec.native\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Audiodec GetAjmContext pointers: %s\n", message);
    std::abort();
}

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* data, std::size_t size) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr, "VirtualAlloc failed");
        std::memcpy(_page, data, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0, "VirtualProtect failed");
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED, "mmap failed");
        std::memcpy(_page, data, size);
        Require(mprotect(_page, _size, PROT_READ) == 0, "mprotect failed");
#endif
    }

    ~ReadOnlyPage() {
        if (_page == nullptr) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    void* Data() const { return _page; }
    const void* Bytes() const { return _page; }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

void TestDecodePointers() {
    alignas(8) unsigned char context[0x40]{};
    std::uint32_t paramBuf[8]{};
    std::uint32_t bsiBuf[16]{};
    unsigned char auData[16]{};
    unsigned char pcmData[16]{};
    AudiodecAuInfo au{};
    au.ui_size = 0x18u;
    au.p_au_addr = auData;
    au.ui_au_size = sizeof(auData);
    AudiodecPcmItem pcm{};
    pcm.ui_size = 0x18u;
    pcm.p_pcm_addr = pcmData;
    pcm.ui_pcm_size = sizeof(pcmData);

    auto* const badCtrl = reinterpret_cast<AudiodecCtrl*>(0x10);
    errno = 71;
    Require(sceAudiodecDecode2(1u, badCtrl) == InvalidCtrlPointer && errno == 71, "Decode2 bad ctrl");
    errno = 71;
    Require(sceAudiodecDecode2Ex(context, 1u, badCtrl) == InvalidCtrlPointer && errno == 71, "Decode2Ex bad ctrl");
    errno = 71;
    Require(sceAudiodecDecode2WithPriority(1u, badCtrl, 40) == InvalidCtrlPointer && errno == 71, "Decode2WithPriority bad ctrl");
    errno = 71;
    Require(sceAudiodecDecode2WithPriorityEx(context, 1u, badCtrl, 40) == InvalidCtrlPointer && errno == 71, "Decode2WithPriorityEx bad ctrl");
    errno = 71;
    Require(sceAudiodecDecodeEx(context, 1u, badCtrl) == InvalidCtrlPointer && errno == 71, "DecodeEx bad ctrl");
    errno = 71;
    Require(sceAudiodecDecodeWithPriority(1u, badCtrl, 40) == InvalidCtrlPointer && errno == 71, "DecodeWithPriority bad ctrl");
    errno = 71;
    Require(sceAudiodecDecodeWithPriorityEx(context, 1u, badCtrl, 40) == InvalidCtrlPointer && errno == 71, "DecodeWithPriorityEx bad ctrl");

    AudiodecCtrl ctrlBadAu{};
    ctrlBadAu.pParam = paramBuf;
    ctrlBadAu.pBsiInfo = bsiBuf;
    ctrlBadAu.pAuInfo = reinterpret_cast<AudiodecAuInfo*>(0x10);
    ctrlBadAu.pPcmItem = &pcm;
    errno = 71;
    Require(sceAudiodecDecode2(1u, &ctrlBadAu) == InvalidAuInfoPointer && errno == 71, "Decode2 bad pAuInfo");
    errno = 71;
    Require(sceAudiodecDecode2Ex(context, 1u, &ctrlBadAu) == InvalidAuInfoPointer && errno == 71, "Decode2Ex bad pAuInfo");
    errno = 71;
    Require(sceAudiodecDecodeWithPriorityEx(context, 1u, &ctrlBadAu, 40) == InvalidAuInfoPointer && errno == 71, "DecodeWithPriorityEx bad pAuInfo");

    AudiodecCtrl ctrlBadPcm{};
    ctrlBadPcm.pParam = paramBuf;
    ctrlBadPcm.pBsiInfo = bsiBuf;
    ctrlBadPcm.pAuInfo = &au;
    ctrlBadPcm.pPcmItem = reinterpret_cast<AudiodecPcmItem*>(0x10);
    errno = 71;
    Require(sceAudiodecDecodeEx(context, 1u, &ctrlBadPcm) == InvalidPcmItemPointer && errno == 71, "DecodeEx bad pPcmItem");
    errno = 71;
    Require(sceAudiodecDecode2WithPriority(1u, &ctrlBadPcm, 40) == InvalidPcmItemPointer && errno == 71, "Decode2WithPriority bad pPcmItem");
    errno = 71;
    Require(sceAudiodecDecode2WithPriorityEx(context, 1u, &ctrlBadPcm, 40) == InvalidPcmItemPointer && errno == 71, "Decode2WithPriorityEx bad pPcmItem");

    AudiodecPcmItem nullAddrPcm = pcm;
    nullAddrPcm.p_pcm_addr = nullptr;
    ReadOnlyPage roPcmPage(&nullAddrPcm, sizeof(nullAddrPcm));
    AudiodecCtrl ctrlReadOnly{};
    ctrlReadOnly.pParam = paramBuf;
    ctrlReadOnly.pBsiInfo = bsiBuf;
    ctrlReadOnly.pAuInfo = &au;
    ctrlReadOnly.pPcmItem = static_cast<AudiodecPcmItem*>(roPcmPage.Data());
    ReadOnlyPage roCtrlPage(&ctrlReadOnly, sizeof(ctrlReadOnly));
    errno = 71;
    Require(sceAudiodecDecode2Ex(context, 1u, static_cast<AudiodecCtrl*>(roCtrlPage.Data())) == InvalidPcmPointer && errno == 71, "Decode2Ex read-only ctrl/pcm");
}

void TestRegisterAndUnregisterCodecPointers(UnregisterCodecFn unregisterCodec) {
    const std::uint32_t codecTypes[] = {11u, 12u};
    for (std::uint32_t codecType : codecTypes) {
        errno = 71;
        Require(sceAudiodecRegisterCodec(reinterpret_cast<void*>(0x10), codecType) == InvalidArgument && errno == 71, "RegisterCodec bad context");
        errno = 71;
        Require(sceAudiodecRegisterCodec(reinterpret_cast<void*>(UINTPTR_MAX - 8u), codecType) == InvalidArgument && errno == 71, "RegisterCodec wrap context");
        errno = 71;
        Require(unregisterCodec(reinterpret_cast<const void*>(0x10), codecType) == InvalidArgument && errno == 71, "UnregisterCodec bad context");
        errno = 71;
        Require(unregisterCodec(reinterpret_cast<const void*>(UINTPTR_MAX - 8u), codecType) == InvalidArgument && errno == 71, "UnregisterCodec wrap context");

        alignas(8) unsigned char otherSet[0x40]{};
        const std::uint32_t one = 1u;
        const std::size_t otherOffset = codecType == 11u ? 0x14u : 0x10u;
        std::memcpy(otherSet + otherOffset, &one, sizeof(one));
        ReadOnlyPage roOtherSet(otherSet, sizeof(otherSet));
        errno = 71;
        Require(sceAudiodecRegisterCodec(roOtherSet.Data(), codecType) == InvalidArgument && errno == 71, "RegisterCodec read-only context write rejected");
        Require(std::memcmp(roOtherSet.Bytes(), otherSet, sizeof(otherSet)) == 0, "RegisterCodec modified read-only context");

        alignas(8) unsigned char selfSet[0x40]{};
        const std::size_t selfOffset = codecType == 11u ? 0x10u : 0x14u;
        std::memcpy(selfSet + selfOffset, &one, sizeof(one));
        ReadOnlyPage roSelfSet(selfSet, sizeof(selfSet));
        errno = 71;
        Require(sceAudiodecRegisterCodec(roSelfSet.Data(), codecType) == AlreadyRegistered && errno == 71, "RegisterCodec read-only already registered");

        alignas(8) unsigned char emptyContext[0x40]{};
        ReadOnlyPage roEmpty(emptyContext, sizeof(emptyContext));
        errno = 71;
        Require(unregisterCodec(roEmpty.Bytes(), codecType) == NotRegistered && errno == 71, "UnregisterCodec read-only not registered");
    }
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    const auto getAjmContext = Resolve<GetAjmContextFn>("sceAudiodecGetAjmContext");
    const auto unregisterCodec = Resolve<UnregisterCodecFn>("sceAudiodecUnregisterCodec");
    alignas(8) unsigned char context[0x40];
    std::memset(context, 0xee, sizeof(context));
    const std::uint32_t ids[2] = {0x12345678u, 0x9abcdef0u};
    std::memcpy(context, ids, sizeof(ids));

    unsigned char before[sizeof(context)];
    std::memcpy(before, context, sizeof(before));

    std::uint32_t ajmi = Untouched;
    std::uint32_t ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, &ajmi, &ajm) == 0 && ajmi == ids[0] && ajm == ids[1], "the two ids were not read from offsets 0 and 4");
    Require(errno == 71, "errno was modified on valid context read/write");
    Require(std::memcmp(before, context, sizeof(context)) == 0, "a valid call wrote the context");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, nullptr, nullptr) == 0 && ajmi == Untouched && ajm == Untouched, "two null outputs were not skipped");
    Require(errno == 71, "errno was modified with null outputs");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(reinterpret_cast<const void*>(0x10), &ajmi, &ajm) == InvalidArgument && ajmi == Untouched && ajm == Untouched, "a dangling context was not rejected");
    Require(errno == 71, "errno was modified on dangling context");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, reinterpret_cast<std::uint32_t*>(0x10), &ajm) == InvalidArgument && ajmi == Untouched && ajm == Untouched, "a dangling Ajmi output was not rejected");
    Require(errno == 71, "errno was modified on dangling Ajmi output");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, &ajmi, reinterpret_cast<std::uint32_t*>(0x10)) == InvalidArgument && ajmi == ids[0] && ajm == Untouched, "a dangling Ajm output was not rejected");
    Require(errno == 71, "errno was modified on dangling Ajm output");

    errno = 71;
    Require(sceAudiodecGetContext(context) == 0 && errno == 71, "module context preserves errno on first access");
    ReadOnlyPage readOnly(before, sizeof(before));
    errno = 71;
    Require(sceAudiodecGetContext(readOnly.Data()) == InvalidArgument && errno == 71 &&
            std::memcmp(readOnly.Bytes(), before, sizeof(before)) == 0, "read-only module snapshot output");
    for (std::uint32_t handle : {1u, 6u << 14}) {
        errno = 71;
        Require(sceAudiodecDeleteDecoderEx(reinterpret_cast<void*>(0x10), handle) == InvalidArgument && errno == 71,
                "unreadable decoder context");
    }

    TestDecodePointers();
    TestRegisterAndUnregisterCodecPointers(unregisterCodec);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("Audiodec GetAjmContext pointer tests passed");
    return 0;
}
