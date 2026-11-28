#include "prx/libSceAudiodec.native/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libSceAjmi/include/InventoryExports.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include <algorithm>
#include <array>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/ExportLookup.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>

namespace {

constexpr int ErrorInvalidType = static_cast<int>(0x807F0001u);
constexpr int ErrorArgument = static_cast<int>(0x807F0002u);
constexpr int ErrorInvalidParamSize = static_cast<int>(0x807F0004u);
constexpr int ErrorInvalidBsiInfoSize = static_cast<int>(0x807F0005u);
constexpr int ErrorInvalidAuInfoSize = static_cast<int>(0x807F0006u);
constexpr int ErrorInvalidPcmItemSize = static_cast<int>(0x807F0007u);
constexpr int ErrorInvalidCtrlPointer = static_cast<int>(0x807F0008u);
constexpr int ErrorInvalidParamPointer = static_cast<int>(0x807F0009u);
constexpr int ErrorInvalidBsiInfoPointer = static_cast<int>(0x807F000Au);
constexpr int ErrorInvalidAuInfoPointer = static_cast<int>(0x807F000Bu);
constexpr int ErrorInvalidPcmItemPointer = static_cast<int>(0x807F000Cu);
constexpr int ErrorInvalidAuPointer = static_cast<int>(0x807F000Du);
constexpr int ErrorInvalidPcmPointer = static_cast<int>(0x807F000Eu);
constexpr int ErrorInvalidAuSize = static_cast<int>(0x807F0011u);
constexpr int ErrorInvalidPcmSize = static_cast<int>(0x807F0012u);
constexpr int ErrorInvalidPriority = static_cast<int>(0x807FF000u);
constexpr int ErrorAlreadyRegistered = static_cast<int>(0x80930009u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "GuestAudiodecNative: %s\n", message);
    std::abort();
}

void CheckDecoderRelease(void* context, std::uint32_t ajmi, std::uint32_t ajm) {
    for (std::uint32_t codec : {0u, 1u, 2u, 24u, 3u, 4u, 12u, 23u}) {
        const bool native = codec == 0 || codec == 1 || codec == 2 || codec == 24;
        const std::uint32_t owner = native ? ajm : ajmi;
        Require(sceAjmModuleRegister(owner, codec, 0) == 0, "register decoder codec");
        std::uint32_t instance = 0;
        const int created = native ? sceAjmInstanceCreate(owner, codec, 0, &instance) :
                                     sceAjmiInstanceCreate(owner, codec, 0, &instance);
        Require(created == 0 && sceAjmInstanceCodecType(instance) == codec, "live decoder");
        std::uint32_t swapped[] = {ajm, ajmi};
        Require(sceAudiodecDeleteDecoderEx(swapped, instance) == static_cast<int>(0x807f000fu),
                "wrong owner preserves live decoder");
        Require(sceAudiodecDeleteDecoderEx(context, instance) == 0, "delete using selected service");
        Require(sceAjmInstanceDestroy(owner, instance) == static_cast<int>(0x80930003u), "backend released decoder");
        Require(sceAudiodecDeleteDecoderEx(context, instance) == static_cast<int>(0x807f000fu), "map missing instance");
    }
    for (std::uint32_t codec : {6u, 9u, 20u, 25u, 0x3ffffu}) {
        std::uint32_t onlyAjm[] = {0, ajm};
        Require(sceAudiodecDeleteDecoderEx(onlyAjm, codec << 14) == static_cast<int>(0x80930002u),
                "other codec ids select the Ajmi context");
    }
}

}

int main(int argc, char** argv) {
    const char* libraryPath = argc > 1 ? argv[1] : nullptr;
    if (libraryPath != nullptr) {
        struct ExportEntry {
            const char* name;
        };
        const ExportEntry exports[] = {
            {"sceAudiodecClearContextEx"},
            {"sceAudiodecCreateDecoderEx"},
            {"sceAudiodecDecode2"},
            {"sceAudiodecDecode2Ex"},
            {"sceAudiodecDecode2WithPriority"},
            {"sceAudiodecDecode2WithPriorityEx"},
            {"sceAudiodecDecodeEx"},
            {"sceAudiodecDecodeWithPriority"},
            {"sceAudiodecDecodeWithPriorityEx"},
            {"sceAudiodecDeleteDecoderEx"},
            {"sceAudiodecGetContext"},
            {"sceAudiodecInitialize"},
            {"sceAudiodecRegisterCodec"},
            {"sceAudiodecTerminate"},
        };
#ifdef _WIN32
        void* library = LoadLibraryA(libraryPath);
#else
        void* library = dlopen(libraryPath, RTLD_NOW | RTLD_LOCAL);
#endif
        Require(library != nullptr, "failed to load module library");
        for (const auto& entry : exports) {
            Require(LibraryExportsName(library, entry.name), entry.name);
        }
        Require(!LibraryExportsName(library, "sceAudiodecNotExported"), "absent export resolved");
    }

    alignas(8) unsigned char context[64];
    std::memset(context, 0, sizeof(context));

    Require(sceAudiodecGetContext(nullptr) == ErrorArgument, "GetContext null out");
    std::array<unsigned char, 32> reference{};
    Require(sceAudiodecGetContext(reference.data()) == 0, "module context");
    for (unsigned displacement = 0; displacement < 8; ++displacement) {
        alignas(8) std::array<unsigned char, 48> output;
        output.fill(0xa5);
        auto* snapshot = output.data() + displacement;
        Require(sceAudiodecGetContext(snapshot) == 0 && std::memcmp(reference.data(), snapshot, 32) == 0,
                "stable 32-byte snapshot at every alignment");
        Require(std::all_of(output.data(), snapshot, [](auto b) { return b == 0xa5; }) &&
                std::all_of(snapshot + 32, output.data() + output.size(), [](auto b) { return b == 0xa5; }),
                "copy preserves surrounding bytes");
    }
    std::uint32_t moduleAjmi = 0, moduleAjm = 0;
    Require(sceAudiodecGetAjmContext(reference.data(), &moduleAjmi, &moduleAjm) == 0 &&
            moduleAjmi && moduleAjm && moduleAjmi != moduleAjm, "module service ids");
    PthreadRwlock moduleLock = nullptr;
    std::memcpy(&moduleLock, reference.data() + 12, sizeof(moduleLock));
    Require(moduleLock && scePthreadRwlockTryrdlock(&moduleLock) == 0 && scePthreadRwlockUnlock(&moduleLock) == 0,
            "module lock lies at firmware offset");
    Require(std::all_of(reference.begin() + 20, reference.end(), [](auto b) { return b == 0; }),
            "module flags and trailing bytes");
    CheckDecoderRelease(reference.data(), moduleAjmi, moduleAjm);

    Require(sceAudiodecInitialize(nullptr) == ErrorArgument, "Initialize null context");
    alignas(8) unsigned char fresh[64];
    std::memset(fresh, 0xa5, sizeof(fresh));
    Require(sceAudiodecInitialize(fresh) == 0, "Initialize caller context");
    std::uint32_t freshAjmi = 0, freshAjm = 0;
    Require(sceAudiodecGetAjmContext(fresh, &freshAjmi, &freshAjm) == 0 && freshAjmi && freshAjm &&
            freshAjmi != freshAjm && freshAjmi != moduleAjmi && freshAjm != moduleAjm, "Initialize opens both services");
    auto* freshLock = reinterpret_cast<unsigned char*>((reinterpret_cast<std::uintptr_t>(fresh) + 0xFu) & ~static_cast<std::uintptr_t>(7u));
    PthreadRwlock freshRwlock = nullptr;
    std::memcpy(&freshRwlock, freshLock, sizeof(freshRwlock));
    Require(freshRwlock && scePthreadRwlockTryrdlock(&freshRwlock) == 0 && scePthreadRwlockUnlock(&freshRwlock) == 0,
            "Initialize creates the lock");
    Require(std::all_of(freshLock + 8, freshLock + 16, [](auto b) { return b == 0; }), "Initialize clears both codec flags");
    Require(std::all_of(freshLock + 16, fresh + sizeof(fresh), [](auto b) { return b == 0xa5; }), "Initialize writes nothing past the flags");
    Require(scePthreadRwlockDestroy(&freshRwlock) == 0 && sceAjmiFinalize(freshAjmi) == 0 && sceAjmFinalize(freshAjm) == 0,
            "release the caller context");
    Require(sceAudiodecInitialize(reinterpret_cast<void*>(0x10)) == static_cast<int>(0x80930005u), "Initialize unwritable context");

    bool termNullCaught = false;
    try {
        static_cast<void>(sceAudiodecTerminate(nullptr));
    } catch (const std::runtime_error&) {
        termNullCaught = true;
    }
    Require(!termNullCaught, "Terminate null context returns before service");

    Require(sceAudiodecRegisterCodec(nullptr, 1) == ErrorArgument, "RegisterCodec null context");
    for (std::uint32_t invalidType : {0u, 5u, 7u, 8u, 9u, 13u, 100u}) {
        Require(sceAudiodecRegisterCodec(context, invalidType) == ErrorInvalidType, "RegisterCodec invalid type");
    }

    auto* lock = reinterpret_cast<unsigned char*>((reinterpret_cast<std::uintptr_t>(context) + 0xFu) & ~static_cast<std::uintptr_t>(7u));
    std::uint32_t flagVal = 1;
    std::memcpy(lock + 8, &flagVal, sizeof(flagVal));
    Require(sceAudiodecRegisterCodec(context, 11) == ErrorAlreadyRegistered, "RegisterCodec 11 self flag");
    std::memset(lock, 0, 16);
    std::memcpy(lock + 0xC, &flagVal, sizeof(flagVal));
    Require(sceAudiodecRegisterCodec(context, 11) == 0, "RegisterCodec 11 other flag");
    std::memcpy(&flagVal, lock + 8, sizeof(flagVal));
    Require(flagVal == 1, "RegisterCodec 11 flag written");

    std::memset(lock, 0, 16);
    flagVal = 1;
    std::memcpy(lock + 0xC, &flagVal, sizeof(flagVal));
    Require(sceAudiodecRegisterCodec(context, 12) == ErrorAlreadyRegistered, "RegisterCodec 12 self flag");
    std::memset(lock, 0, 16);
    std::memcpy(lock + 8, &flagVal, sizeof(flagVal));
    Require(sceAudiodecRegisterCodec(context, 12) == 0, "RegisterCodec 12 other flag");
    std::memcpy(&flagVal, lock + 0xC, sizeof(flagVal));
    Require(flagVal == 1, "RegisterCodec 12 flag written");

    AudiodecCtrl ctrl{};
    std::uint32_t paramBuffer[8]{};
    std::uint32_t bsiInfoBuffer[16]{};
    AudiodecAuInfo auInfo{};
    AudiodecPcmItem pcmItem{};
    unsigned char auData[64]{};
    unsigned char pcmData[64]{};

    Require(sceAudiodecCreateDecoderEx(nullptr, &ctrl, 1) == ErrorArgument, "CreateDecoderEx null context");
    Require(sceAudiodecCreateDecoderEx(context, nullptr, 1) == ErrorInvalidCtrlPointer, "CreateDecoderEx null ctrl");
    errno = 71;
    Require(sceAudiodecCreateDecoderEx(context, reinterpret_cast<AudiodecCtrl*>(0x10), 1) == ErrorInvalidCtrlPointer && errno == 71, "CreateDecoderEx unreadable ctrl");
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidParamPointer, "CreateDecoderEx null param");
    ctrl.pParam = reinterpret_cast<void*>(0x10);
    ctrl.pBsiInfo = bsiInfoBuffer;
    errno = 71;
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidParamPointer && errno == 71, "CreateDecoderEx unreadable param");
    ctrl.pParam = paramBuffer;
    ctrl.pBsiInfo = nullptr;
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidBsiInfoPointer, "CreateDecoderEx null bsi");
    ctrl.pBsiInfo = reinterpret_cast<void*>(0x10);
    paramBuffer[0] = 0xC;
    errno = 71;
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidBsiInfoPointer && errno == 71, "CreateDecoderEx unreadable bsi");
    ctrl.pBsiInfo = bsiInfoBuffer;
    for (std::uint32_t invalidType : {0u, 5u, 7u, 8u, 9u, 13u, 20u}) {
        Require(sceAudiodecCreateDecoderEx(context, &ctrl, invalidType) == ErrorInvalidType, "CreateDecoderEx invalid type");
    }
    paramBuffer[0] = 8;
    bsiInfoBuffer[0] = 0x24;
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidParamSize, "CreateDecoderEx bad param size");
    paramBuffer[0] = 0xC;
    bsiInfoBuffer[0] = 16;
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidBsiInfoSize, "CreateDecoderEx bad bsi size");

    Require(sceAudiodecDeleteDecoderEx(nullptr, 1) == ErrorArgument, "DeleteDecoderEx null context");
    Require(sceAudiodecClearContextEx(nullptr, 1) == ErrorArgument, "ClearContextEx null context");

    auto testDecodeChecks = [&](auto decodeFn, auto isExTag, auto hasPriorityTag) {
        constexpr bool isEx = decltype(isExTag)::value;
        constexpr bool hasPriority = decltype(hasPriorityTag)::value;

        AudiodecCtrl c{};
        std::uint32_t pBuf[8]{};
        std::uint32_t bBuf[16]{};
        AudiodecAuInfo au{};
        AudiodecPcmItem pcm{};
        unsigned char aData[32]{};
        unsigned char pData[32]{};

        auto call = [&](AudiodecCtrl* targetCtrl, std::int32_t priority = 40) {
            if constexpr (isEx && hasPriority) {
                return decodeFn(context, 1u, targetCtrl, priority);
            } else if constexpr (isEx && !hasPriority) {
                return decodeFn(context, 1u, targetCtrl);
            } else if constexpr (!isEx && hasPriority) {
                return decodeFn(1u, targetCtrl, priority);
            } else {
                return decodeFn(1u, targetCtrl);
            }
        };

        if constexpr (isEx) {
            if constexpr (hasPriority) {
                Require(decodeFn(nullptr, 1u, &c, 40) == ErrorArgument, "Decode null context");
            } else {
                Require(decodeFn(nullptr, 1u, &c) == ErrorArgument, "Decode null context");
            }
        }
        Require(call(nullptr) == ErrorInvalidCtrlPointer, "Decode null ctrl");
        Require(call(&c) == ErrorInvalidParamPointer, "Decode null pParam");
        c.pParam = pBuf;
        Require(call(&c) == ErrorInvalidBsiInfoPointer, "Decode null pBsiInfo");
        c.pBsiInfo = bBuf;
        Require(call(&c) == ErrorInvalidAuInfoPointer, "Decode null pAuInfo");
        c.pAuInfo = &au;
        Require(call(&c) == ErrorInvalidAuPointer, "Decode null p_au_addr");
        au.p_au_addr = aData;
        Require(call(&c) == ErrorInvalidPcmItemPointer, "Decode null pPcmItem");
        c.pPcmItem = &pcm;
        Require(call(&c) == ErrorInvalidPcmPointer, "Decode null p_pcm_addr");
        pcm.p_pcm_addr = pData;

        au.ui_au_size = 0;
        pcm.ui_pcm_size = 100;
        Require(call(&c) == ErrorInvalidAuSize, "Decode zero au size");
        au.ui_au_size = 0xFFFFFFFFu;
        Require(call(&c) == ErrorInvalidAuSize, "Decode max au size");
        au.ui_au_size = 100;

        pcm.ui_pcm_size = 0;
        Require(call(&c) == ErrorInvalidPcmSize, "Decode zero pcm size");
        pcm.ui_pcm_size = 0xFFFFFFFFu;
        Require(call(&c) == ErrorInvalidPcmSize, "Decode max pcm size");
        pcm.ui_pcm_size = 100;

        au.ui_size = 0x10;
        pcm.ui_size = 0x18;
        Require(call(&c) == ErrorInvalidAuInfoSize, "Decode bad au ui_size");
        au.ui_size = 0x18;

        pcm.ui_size = 0x14;
        Require(call(&c) == ErrorInvalidPcmItemSize, "Decode bad pcm ui_size");
        pcm.ui_size = 0x18;

        if constexpr (hasPriority) {
            Require(call(&c, 19) == ErrorInvalidPriority, "Decode priority 19");
            Require(call(&c, 101) == ErrorInvalidPriority, "Decode priority 101");
            Require(call(&c, -1) == ErrorInvalidPriority, "Decode priority -1");
        }

        bool caught = false;
        try {
            call(&c);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        Require(caught, "Decode reached unimplemented Ajm service");
    };

    testDecodeChecks(sceAudiodecDecode2, std::false_type{}, std::false_type{});
    testDecodeChecks(sceAudiodecDecode2Ex, std::true_type{}, std::false_type{});
    testDecodeChecks(sceAudiodecDecode2WithPriority, std::false_type{}, std::true_type{});
    testDecodeChecks(sceAudiodecDecode2WithPriorityEx, std::true_type{}, std::true_type{});
    testDecodeChecks(sceAudiodecDecodeEx, std::true_type{}, std::false_type{});
    testDecodeChecks(sceAudiodecDecodeWithPriority, std::false_type{}, std::true_type{});
    testDecodeChecks(sceAudiodecDecodeWithPriorityEx, std::true_type{}, std::true_type{});

    bool termCaught = false;
    try {
        sceAudiodecTerminate(context);
    } catch (const std::runtime_error&) {
        termCaught = true;
    }
    Require(termCaught, "Terminate reaches service");

    bool clearCaught = false;
    try {
        sceAudiodecClearContextEx(context, 1);
    } catch (const std::runtime_error&) {
        clearCaught = true;
    }
    Require(clearCaught, "ClearContextEx reaches service");

    std::uint32_t noServices[2]{};
    Require(sceAudiodecDeleteDecoderEx(noServices, 1) == static_cast<int>(0x80930002u), "propagate invalid service context");

    paramBuffer[0] = 0xC;
    bsiInfoBuffer[0] = 0x24;
    bool createCaught = false;
    try {
        sceAudiodecCreateDecoderEx(context, &ctrl, 1);
    } catch (const std::runtime_error&) {
        createCaught = true;
    }
    Require(createCaught, "CreateDecoderEx reaches service");

    std::memset(lock, 0, 16);
    bool regCaught = false;
    try {
        sceAudiodecRegisterCodec(context, 1);
    } catch (const std::runtime_error&) {
        regCaught = true;
    }
    Require(regCaught, "RegisterCodec reaches service");

    std::puts("libSceAudiodec.native tests passed");
    return 0;
}
