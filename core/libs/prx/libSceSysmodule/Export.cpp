#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct ModuleEntry {
    std::uint32_t id;
    const char* name;
};

constexpr ModuleEntry kModuleTable[] = {
    {0x00000001, "libSceAudioOut"},
    {0x00000002, "libSceAudioIn"},
    {0x00000006, "libSceFiber"},
    {0x00000007, "libSceUlt"},
    {0x0000000b, "libSceNgs2"},
    {0x00000017, "libSceXml"},
    {0x00000019, "libSceNpUtility"},
    {0x0000001a, "libSceVoice"},
    {0x0000001b, "libSceVoiceQos"},
    {0x0000001c, "libSceNpMatching2"},
    {0x0000001e, "libSceNpScoreRanking"},
    {0x00000021, "libSceRudp"},
    {0x0000002c, "libSceNpTus"},
    {0x00000038, "libSceFace"},
    {0x00000039, "libSceSmart"},
    {0x00000080, "libSceJson"},
    {0x00000081, "libSceGameLiveStreaming"},
    {0x00000082, "libSceCompanionUtil"},
    {0x00000083, "libScePlayGo"},
    {0x00000084, "libSceFont"},
    {0x00000085, "libSceVideoRecording"},
    {0x00000088, "libSceAudiodec"},
    {0x0000008a, "libSceJpegDec"},
    {0x0000008b, "libSceJpegEnc"},
    {0x0000008c, "libScePngDec"},
    {0x0000008d, "libScePngEnc"},
    {0x0000008e, "libSceVideodec"},
    {0x0000008f, "libSceMove"},
    {0x00000091, "libScePadTracker"},
    {0x00000092, "libSceDepth"},
    {0x00000093, "libSceHand"},
    {0x00000095, "libSceIme"},
    {0x00000096, "libSceImeDialog"},
    {0x00000097, "libSceNpParty"},
    {0x00000098, "libSceFontFt"},
    {0x00000099, "libSceFreeTypeOt"},
    {0x0000009a, "libSceFreeTypeOl"},
    {0x0000009b, "libSceFreeTypeOptOl"},
    {0x0000009c, "libSceScreenShot"},
    {0x0000009d, "libSceNpAuth"},
    {0x000000a4, "libSceMsgDialog"},
    {0x000000a5, "libSceAvPlayer"},
    {0x000000a6, "libSceContentExport"},
    {0x000000a7, "libSceAudio3d"},
    {0x000000a8, "libSceNpCommerce"},
    {0x000000a9, "libSceMouse"},
    {0x000000aa, "libSceCompanionHttpd"},
    {0x000000ab, "libSceWebBrowserDialog"},
    {0x000000ac, "libSceErrorDialog"},
    {0x000000ad, "libSceNpTrophy"},
    {0x000000ae, "libSceVideoCoreInterface"},
    {0x000000af, "libSceVideoCoreServerInterface"},
    {0x000000b0, "libSceNpSnsFacebookDialog"},
    {0x000000b1, "libSceMoveTracker"},
    {0x000000b2, "libSceNpProfileDialog"},
    {0x000000b3, "libSceNpFriendListDialog"},
    {0x000000b4, "libSceAppContent"},
    {0x000000b5, "libSceNpSignaling"},
    {0x000000b6, "libSceRemoteplay"},
    {0x000000b7, "libSceUsbd"},
    {0x000000b8, "libSceGameCustomDataDialog"},
    {0x000000b9, "libSceNpEulaDialog"},
    {0x000000ba, "libSceRandom"},
    {0x000000bc, "libSceM4aacEnc"},
    {0x000000bd, "libSceAudiodecCpu"},
    {0x000000d7, "libSceDiscMap"},
    {0x00000106, "libSceKeyboard"},
    {0x80000001, "libSceAudioOut"},
    {0x80000002, "libSceAudioIn"},
    {0x80000003, "libSceAvcap"},
    {0x80000004, "libSceSysCore"},
    {0x80000007, "libSceCdlgUtilServer"},
    {0x80000009, "libSceNetCtl"},
    {0x8000000a, "libSceHttp"},
    {0x8000000b, "libSceSsl2"},
    {0x8000000c, "libSceNpCommon"},
    {0x8000000d, "libSceNpManager"},
    {0x8000000e, "libSceNpWebApi"},
    {0x8000000f, "libSceSaveData"},
    {0x80000010, "libSceSystemService"},
    {0x80000011, "libSceUserService"},
    {0x80000012, "libSceVisionManager"},
    {0x80000013, "libSceAc3Enc"},
    {0x80000014, "libSceAppInstUtil"},
    {0x80000015, "libSceVdecCore"},
    {0x80000016, "libSceVencCore"},
    {0x80000017, "libSceHidControl"},
    {0x80000018, "libSceCommonDialog"},
    {0x80000019, "libScePerf"},
    {0x8000001a, "libSceCamera"},
    {0x8000001b, "libSceNpSns"},
    {0x8000001c, "libSceNet"},
    {0x8000001d, "libSceIpmi"},
    {0x8000001e, "libSceMbus"},
    {0x8000001f, "libSceRegMgr"},
    {0x80000020, "libSceRtc"},
    {0x80000021, "libSceAvSetting"},
    {0x80000022, "libSceVideoOut"},
    {0x80000023, "libSceAjm"},
    {0x80000024, "libScePad"},
    {0x80000025, "libSceDbg"},
    {0x80000026, "libSceSysUtil"},
    {0x80000027, "libSceMarlin"},
    {0x80000028, "libSceDtsEnc"},
    {0x80000029, "libSceDipsw"},
    {0x8000002a, "libSceNpWebApi2"},
    {0x8000003d, "libSceDbgAssist"},
    {0x80000048, "libSceMat"},
    {0x80000052, "libSceGnmDriver"},
    {0x80000075, "libSceRazorCpu"},
    {0x8000008c, "libSceHttp2"},
    {0x8000008d, "libSceNpGameIntent"},
    {0x8000008f, "libSceNpWebApi2"},
};

constexpr std::size_t kModuleTableSize = sizeof(kModuleTable) / sizeof(kModuleTable[0]);

const char* findModuleName(std::uint32_t id) {
    for (std::size_t i = 0; i < kModuleTableSize; i++) {
        if (kModuleTable[i].id == id) {
            return kModuleTable[i].name;
        }
    }
    return nullptr;
}

std::mutex gMutex;
std::unordered_map<std::uint32_t, std::int32_t> gLoadCount;

}

extern "C" {

int sceSysmoduleGetModuleInfoForUnwind(std::uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    (void)flags;
    std::ifstream maps("/proc/self/maps");
    if (!maps) {
        throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: failed to open /proc/self/maps");
    }
    std::string line;
    while (std::getline(maps, line)) {
        std::uint64_t start = 0;
        std::uint64_t end = 0;
        char perms[8] = {};
        std::uint64_t offset = 0;
        int devMajor = 0;
        int devMinor = 0;
        std::uint64_t inode = 0;
        char path[4096] = {};
        int parsed = std::sscanf(
            line.c_str(),
            "%llx-%llx %7s %llx %x:%x %llu %4095s",
            (unsigned long long*)&start,
            (unsigned long long*)&end,
            perms,
            (unsigned long long*)&offset,
            &devMajor,
            &devMinor,
            (unsigned long long*)&inode,
            path
        );
        if (parsed < 7 || addr < start || addr >= end) {
            continue;
        }
        info->st_size = sizeof(ModuleInfoForUnwind);
        std::strncpy(info->name, parsed >= 8 ? path : "", sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        info->eh_frame_hdr_addr = 0;
        info->eh_frame_addr = 0;
        info->eh_frame_size = 0;
        info->seg0_addr = start;
        info->seg0_size = end - start;
        return 0;
    }
    throw std::runtime_error("sceSysmoduleGetModuleInfoForUnwind: address not found in maps");
}

int sceSysmoduleIsLoaded(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleIsLoaded: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleIsLoaded: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return 0x80A90002;
    }
    return 0;
}

int sceSysmoduleLoadModule(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleLoadModule: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleLoadModule: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    return 0;
}

int sceSysmoduleLoadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret) {
    (void)argc;
    (void)argv;
    (void)unk;
    if ((id & 0x7fffffffu) == 0) {
        throw std::runtime_error("sceSysmoduleLoadModuleInternalWithArg: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleLoadModuleInternalWithArg: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gLoadCount[id]++;
    if (ret) {
        *ret = 0;
    }
    return 0;
}

int sceSysmoduleUnloadModule(std::uint16_t id) {
    if (id == 0) {
        throw std::runtime_error("sceSysmoduleUnloadModule: invalid id 0");
    }
    if (!findModuleName(id)) {
        throw std::runtime_error(std::string("sceSysmoduleUnloadModule: unknown id ") + std::to_string(id));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gLoadCount.find(id);
    if (it == gLoadCount.end() || it->second < 1) {
        return 0x80A90003;
    }
    it->second--;
    return 0;
}

}
