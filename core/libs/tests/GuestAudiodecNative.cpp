#include "prx/libSceAudiodec.native/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/ExportLookup.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    unsigned char outContext[32];
    std::memset(outContext, 0xAA, sizeof(outContext));
    bool getContextCaught = false;
    try {
        sceAudiodecGetContext(outContext);
    } catch (const std::runtime_error&) {
        getContextCaught = true;
    }
    Require(getContextCaught, "GetContext reaches service");

    Require(sceAudiodecInitialize(nullptr) == ErrorArgument, "Initialize null context");

    bool termNullCaught = false;
    try {
        sceAudiodecTerminate(nullptr);
    } catch (const std::runtime_error&) {
        termNullCaught = true;
    }
    Require(termNullCaught, "Terminate null context reaches service");

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
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidParamPointer, "CreateDecoderEx null param");
    ctrl.pParam = paramBuffer;
    Require(sceAudiodecCreateDecoderEx(context, &ctrl, 1) == ErrorInvalidBsiInfoPointer, "CreateDecoderEx null bsi");
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

    bool initCaught = false;
    try {
        sceAudiodecInitialize(context);
    } catch (const std::runtime_error&) {
        initCaught = true;
    }
    Require(initCaught, "Initialize reaches service");

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

    bool deleteCaught = false;
    try {
        sceAudiodecDeleteDecoderEx(context, 1);
    } catch (const std::runtime_error&) {
        deleteCaught = true;
    }
    Require(deleteCaught, "DeleteDecoderEx reaches service");

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
