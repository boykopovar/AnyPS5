#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceSysmodule/ModuleTable.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
#ifdef __linux__
#include <cstdio>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

constexpr std::uint16_t kFiberModuleId = 0x0006;
constexpr std::uint16_t kUltModuleId = 0x0007;
constexpr int kInvalidValue = static_cast<int>(0x805A1000);
constexpr int kModuleUnloaded = static_cast<int>(0x805A1001);
constexpr std::uint32_t kInternalFlag = 0x80000000u;
constexpr std::uint16_t kAcmModuleId = 0x010b;
constexpr std::uint16_t kTrophy2ModuleId = 0x0110;
constexpr std::uint16_t kVoiceChatModuleId = 0x0114;
constexpr std::uint16_t kPerfModuleId = 0x0005;
constexpr std::uint16_t kVrSetupDialogModuleId = 0x0130;
constexpr std::uint16_t kProprietaryVoiceChatHelperModuleId = 0x0132;
constexpr std::uint16_t kPsmlModuleId = 0x0137;
constexpr std::uint32_t kAgcDriverModuleId = 0x80000080;
constexpr std::uint32_t kNgs2InternalModuleId = 0x80000090;
constexpr std::uint32_t kAgcModuleId = 0x80000094;
constexpr std::uint32_t kHdrScopesInternalModuleId = 0x800000bd;

#ifdef __linux__
struct Mapping {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::string path;
};

Mapping MapsEntry(std::uint64_t address) {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        unsigned long long start = 0;
        unsigned long long end = 0;
        int prefix = 0;
        const int parsed = std::sscanf(line.c_str(), "%llx-%llx %*s %*x %*x:%*x %*u%n", &start, &end, &prefix);
        if (parsed >= 2 && address >= start && address < end) {
            std::string path;
            if (prefix > 0 && static_cast<std::size_t>(prefix) < line.size()) {
                std::size_t begin = line.find_first_not_of(" \t", static_cast<std::size_t>(prefix));
                if (begin != std::string::npos) path = line.substr(begin, 4095);
            }
            return Mapping{start, end, path};
        }
    }
    std::abort();
}

void RequireUnwindMatchesMaps(const void* pointer) {
    const auto address = reinterpret_cast<std::uint64_t>(pointer);
    const auto expected = MapsEntry(address);
    ModuleInfoForUnwind info{};
    Require(sceSysmoduleGetModuleInfoForUnwind(address, 0, &info) == 0);
    Require(info.st_size == sizeof(ModuleInfoForUnwind));
    Require(info.seg0_addr == expected.start && info.seg0_size == expected.end - expected.start);
    Require(std::string(info.name) == expected.path.substr(0, sizeof(info.name) - 1));
    Require(info.eh_frame_hdr_addr == 0 && info.eh_frame_addr == 0 && info.eh_frame_size == 0);
}

void CheckUnwindInfo() {
    RequireUnwindMatchesMaps(reinterpret_cast<const void*>(&RequireUnwindMatchesMaps));
    RequireUnwindMatchesMaps(reinterpret_cast<const void*>(&sceSysmoduleGetModuleInfoForUnwind));
    int local = 0;
    RequireUnwindMatchesMaps(&local);
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* anonymous = mmap(nullptr, page, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(anonymous != MAP_FAILED);
    RequireUnwindMatchesMaps(anonymous);
    Require(munmap(anonymous, page) == 0);
    void* gap = mmap(nullptr, page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(gap != MAP_FAILED && munmap(gap, page) == 0);
    ModuleInfoForUnwind info{};
    bool threw = false;
    try {
        sceSysmoduleGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(gap), 0, &info);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
#endif

std::uint16_t UnknownModuleId() {
    for (std::uint32_t id = 0xffff; id != 0; --id) {
        if (kModuleTable.find(id) == kModuleTable.end()) return static_cast<std::uint16_t>(id);
    }
    std::abort();
}

void CheckUnknownIds() {
    const std::uint16_t id = UnknownModuleId();
    const std::uint32_t internalId = id | kInternalFlag;
    Require(kModuleTable.find(internalId) == kModuleTable.end());
    for (const std::uint16_t rejected : {id, std::uint16_t{0x0001}, std::uint16_t{0x0080}, std::uint16_t{0x00c5}, std::uint16_t{0x0100}, std::uint16_t{0x8000}}) {
        Require(sceSysmoduleIsLoaded(rejected) == kInvalidValue);
        Require(sceSysmoduleLoadModule(rejected) == kInvalidValue);
        Require(sceSysmoduleIsLoaded(rejected) == kInvalidValue);
        Require(sceSysmoduleUnloadModule(rejected) == kInvalidValue);
    }
    Require(kModuleTable.find(0x0111) == kModuleTable.end());
    Require(sceSysmoduleIsLoaded(0x0111) == 0);
    Require(sceSysmoduleLoadModule(0x0111) == 0);
    Require(sceSysmoduleUnloadModule(0x0111) == 0);
    Require(ThrowsRuntimeError([internalId] { sceSysmoduleLoadModuleInternal(internalId); }));
    Require(ThrowsRuntimeError([internalId] { sceSysmoduleUnloadModuleInternal(internalId); }));
    int result = 0x5a5a5a5a;
    Require(ThrowsRuntimeError([internalId, &result] { sceSysmoduleLoadModuleInternalWithArg(internalId, 0, nullptr, 0, &result); }));
    Require(result == 0x5a5a5a5a);
    int unloadResult = 0x5a5a5a5a;
    Require(ThrowsRuntimeError([internalId, &unloadResult] { sceSysmoduleUnloadModuleInternalWithArg(internalId, 0, nullptr, 0, &unloadResult); }));
    Require(unloadResult == 0x5a5a5a5a);
}

#ifdef _WIN32
void RequireUnwindMatchesRegion(const void* pointer) {
    const auto address = reinterpret_cast<std::uint64_t>(pointer);
    MEMORY_BASIC_INFORMATION expected{};
    Require(VirtualQuery(pointer, &expected, sizeof(expected)) != 0);
    char mapped[4096] = {};
    const DWORD length = GetMappedFileNameA(GetCurrentProcess(), expected.BaseAddress, mapped, sizeof(mapped) - 1);
    mapped[length] = '\0';
    for (int repeat = 0; repeat < 3; ++repeat) {
        ModuleInfoForUnwind info{};
        Require(sceSysmoduleGetModuleInfoForUnwind(address, 0, &info) == 0);
        Require(info.st_size == sizeof(ModuleInfoForUnwind));
        Require(info.seg0_addr == reinterpret_cast<std::uint64_t>(expected.BaseAddress) && info.seg0_size == expected.RegionSize);
        Require(std::strncmp(info.name, mapped, sizeof(info.name) - 1) == 0);
        Require(info.eh_frame_hdr_addr == 0 && info.eh_frame_addr == 0 && info.eh_frame_size == 0);
    }
}

void CheckUnwindInfo() {
    RequireUnwindMatchesRegion(reinterpret_cast<const void*>(&RequireUnwindMatchesRegion));
    RequireUnwindMatchesRegion(reinterpret_cast<const void*>(&sceSysmoduleGetModuleInfoForUnwind));
    RequireUnwindMatchesRegion(reinterpret_cast<const void*>(&GetModuleHandleW));
    const auto* host = reinterpret_cast<const char*>(&GetModuleHandleW);
    MEMORY_BASIC_INFORMATION hostRegion{};
    Require(VirtualQuery(host, &hostRegion, sizeof(hostRegion)) != 0);
    for (const std::size_t step : {std::size_t{0x1000}, std::size_t{0x3000}, std::size_t{0x10000}}) {
        if (hostRegion.RegionSize > step) RequireUnwindMatchesRegion(host + step);
    }
    RequireUnwindMatchesRegion(host + 0x2000);
    RequireUnwindMatchesRegion(host);
    const auto* anchor = reinterpret_cast<const char*>(&RtlPcToFileHeader);
    MEMORY_BASIC_INFORMATION anchorRegion{};
    Require(VirtualQuery(anchor, &anchorRegion, sizeof(anchorRegion)) != 0);
    if (anchorRegion.RegionSize > 0x4000) {
        RequireUnwindMatchesRegion(anchor + 0x3000);
        RequireUnwindMatchesRegion(anchor);
        RequireUnwindMatchesRegion(anchor + 0x1000);
        RequireUnwindMatchesRegion(anchor + 0x2000);
    }
    static int data = 1;
    static const char text[] = "unwind region";
    RequireUnwindMatchesRegion(&data);
    RequireUnwindMatchesRegion(text);
    RequireUnwindMatchesRegion(reinterpret_cast<const void*>(&RequireUnwindMatchesRegion));
    int local = 0;
    RequireUnwindMatchesRegion(&local);
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::size_t page = system.dwPageSize;
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, 3 * page, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(memory != nullptr);
    RequireUnwindMatchesRegion(memory);
    DWORD previous = 0;
    Require(VirtualProtect(memory + 2 * page, page, PAGE_READONLY, &previous) != 0);
    RequireUnwindMatchesRegion(memory);
    RequireUnwindMatchesRegion(memory + 2 * page);
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
    RequireUnwindMatchesRegion(memory);
}
#elif defined(__APPLE__)
void RequireUnwindMatchesRegion(const void* pointer, const char* nameEnd) {
    const auto address = reinterpret_cast<std::uint64_t>(pointer);
    mach_vm_address_t region = address;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t basic{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    Require(mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&basic), &count, &object) == KERN_SUCCESS);
    Require(region <= address && address - region < size);
    ModuleInfoForUnwind info{};
    Require(sceSysmoduleGetModuleInfoForUnwind(address, 0, &info) == 0);
    Require(info.st_size == sizeof(ModuleInfoForUnwind));
    Require(info.seg0_addr == region && info.seg0_size == size);
    const std::string name = info.name;
    if (nameEnd == nullptr) {
        Require(name.empty());
    } else {
        const std::string end = std::string("/") + nameEnd;
        Require(name.size() > end.size() && name.compare(name.size() - end.size(), end.size(), end) == 0);
    }
    Require(info.eh_frame_hdr_addr == 0 && info.eh_frame_addr == 0 && info.eh_frame_size == 0);
}

void CheckUnwindInfo() {
    RequireUnwindMatchesRegion(reinterpret_cast<const void*>(&RequireUnwindMatchesRegion), "guest_sysmodule_tests");
    Dl_info library{};
    Require(dladdr(reinterpret_cast<const void*>(&sceSysmoduleGetModuleInfoForUnwind), &library) != 0 && library.dli_fname != nullptr);
    const std::string libraryPath = library.dli_fname;
    RequireUnwindMatchesRegion(reinterpret_cast<const void*>(&sceSysmoduleGetModuleInfoForUnwind), libraryPath.substr(libraryPath.find_last_of('/') + 1).c_str());
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* anonymous = mmap(nullptr, page, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(anonymous != MAP_FAILED);
    RequireUnwindMatchesRegion(anonymous, nullptr);
    Require(munmap(anonymous, page) == 0);
    void* gap = mmap(nullptr, page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(gap != MAP_FAILED && munmap(gap, page) == 0);
    ModuleInfoForUnwind info{};
    bool threw = false;
    try {
        sceSysmoduleGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(gap), 0, &info);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
#endif

}

void CheckAcceptedIdSet() {
    constexpr std::uint16_t kAccepted[] = {
        0x0005, 0x0006, 0x0007, 0x000b, 0x0017, 0x0019, 0x001a, 0x001b, 0x001c, 0x001e, 0x0021, 0x002c,
        0x0038, 0x0081, 0x0083, 0x0084, 0x0085, 0x0088, 0x008a, 0x008b, 0x008c, 0x008d, 0x0095, 0x0096,
        0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009f, 0x00a0, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
        0x00a8, 0x00a9, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00b4, 0x00b6, 0x00b7, 0x00b9, 0x00ba, 0x00bc,
        0x00bd, 0x00be, 0x00c0, 0x00c1, 0x00c2, 0x00c4, 0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00cc, 0x00cd,
        0x00ce, 0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d8, 0x00da, 0x00de,
        0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e7, 0x00e9, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00fa,
        0x00fb, 0x00fe, 0x00ff, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107, 0x010a, 0x010b, 0x010c, 0x010d,
        0x010e, 0x010f, 0x0110, 0x0111, 0x0112, 0x0113, 0x0114, 0x0115, 0x0116, 0x0117, 0x011a, 0x011b,
        0x011c, 0x011d, 0x011e, 0x011f, 0x0120, 0x0121, 0x0122, 0x0123, 0x0124, 0x0125, 0x0126, 0x0127,
        0x0128, 0x0129, 0x012a, 0x012b, 0x012c, 0x012d, 0x012e, 0x012f, 0x0130, 0x0132, 0x0133, 0x0134,
        0x0135, 0x0136, 0x0137, 0x0138, 0x0139, 0x013a, 0x013b, 0x013c, 0x013d,
    };
    static_assert(sizeof(kAccepted) / sizeof(kAccepted[0]) == 141);
    constexpr std::uint16_t kAcceptedWithoutName[] = {
        0x0111, 0x0117, 0x011c, 0x011d, 0x011e, 0x011f, 0x0121, 0x0122, 0x0123, 0x0124, 0x0129,
        0x012c, 0x012d, 0x012e, 0x0133, 0x0134, 0x0136, 0x013a, 0x013b, 0x013c, 0x013d,
    };
    const auto accepted = [&kAccepted](std::uint16_t id) {
        return std::binary_search(std::begin(kAccepted), std::end(kAccepted), id);
    };
    for (std::uint32_t id = 0; id <= 0xffff; ++id) {
        const std::uint16_t value = static_cast<std::uint16_t>(id);
        Require((sceSysmoduleIsLoaded(value) != kInvalidValue) == accepted(value));
        if (!accepted(value)) {
            Require(sceSysmoduleLoadModule(value) == kInvalidValue);
            Require(sceSysmoduleUnloadModule(value) == kInvalidValue);
        }
    }
    for (const std::uint16_t id : kAcceptedWithoutName) {
        Require(accepted(id));
        Require(kModuleTable.find(id) == kModuleTable.end());
        Require(sceSysmoduleIsLoaded(id) == 0);
        Require(sceSysmoduleLoadModule(id) == 0);
        Require(sceSysmoduleUnloadModule(id) == 0);
    }
}

int main() {
    CheckUnknownIds();
    CheckAcceptedIdSet();
    Require(sceSysmoduleIsLoaded(0) == kInvalidValue);
    Require(sceSysmoduleLoadModule(0) == kInvalidValue);
    Require(sceSysmoduleUnloadModule(0) == kInvalidValue);
    for (std::uint32_t id : {0u, kInternalFlag}) {
        Require(sceSysmoduleIsLoadedInternal(id) == kInvalidValue);
        Require(sceSysmoduleLoadModuleInternal(id) == kInvalidValue);
        Require(sceSysmoduleUnloadModuleInternal(id) == kInvalidValue);
        int result = 0x5a5a5a5a;
        Require(sceSysmoduleLoadModuleInternalWithArg(id, 0, nullptr, 0, &result) == kInvalidValue);
        Require(result == 0x5a5a5a5a);
    }

    Require(ThrowsRuntimeError([] { sceSysmoduleIsLoadedInternal(0x00ffffffu); }));
    Require(sceSysmoduleIsLoadedInternal(kFiberModuleId) == kModuleUnloaded);
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleIsLoadedInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == 0);
    Require(sceSysmoduleLoadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModuleInternal(kFiberModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kUltModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kUltModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kUltModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kUltModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kUltModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kUltModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kAcmModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kAcmModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kAcmModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kAcmModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kAcmModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kAcmModuleId) == kModuleUnloaded);

    constexpr std::uint16_t kShareModuleId = 0x010f;
    Require(sceSysmoduleIsLoaded(kShareModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kShareModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kShareModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kShareModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kShareModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kTrophy2ModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModuleInternal(kTrophy2ModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kTrophy2ModuleId) == 0);
    Require(sceSysmoduleLoadModuleInternal(kTrophy2ModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kTrophy2ModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kTrophy2ModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kTrophy2ModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kTrophy2ModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModuleInternal(kTrophy2ModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kVoiceChatModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kVoiceChatModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kVoiceChatModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kVoiceChatModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kVoiceChatModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kVoiceChatModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kPsmlModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kPsmlModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kPsmlModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kPsmlModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kPsmlModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kPsmlModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kProprietaryVoiceChatHelperModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kProprietaryVoiceChatHelperModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kProprietaryVoiceChatHelperModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kProprietaryVoiceChatHelperModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kProprietaryVoiceChatHelperModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kProprietaryVoiceChatHelperModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kPerfModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kPerfModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kPerfModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kPerfModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kPerfModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kPerfModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kVrSetupDialogModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kVrSetupDialogModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kVrSetupDialogModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kVrSetupDialogModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kVrSetupDialogModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kVrSetupDialogModuleId) == kModuleUnloaded);

    Require(sceSysmoduleUnloadModuleInternal(kAgcDriverModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModuleInternal(kAgcDriverModuleId) == 0);
    Require(sceSysmoduleLoadModuleInternal(kAgcDriverModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kAgcDriverModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kAgcDriverModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kAgcDriverModuleId) == kModuleUnloaded);

    Require(sceSysmoduleUnloadModuleInternal(kNgs2InternalModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModuleInternal(kNgs2InternalModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kNgs2InternalModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kNgs2InternalModuleId) == kModuleUnloaded);

    Require(sceSysmoduleUnloadModuleInternal(kHdrScopesInternalModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModuleInternal(kHdrScopesInternalModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kHdrScopesInternalModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kHdrScopesInternalModuleId) == kModuleUnloaded);

    int unloadArgResult = -1;
    Require(sceSysmoduleUnloadModuleInternalWithArg(0, 0, nullptr, 0, &unloadArgResult) == kInvalidValue);
    Require(unloadArgResult == -1);
    Require(sceSysmoduleUnloadModuleInternalWithArg(kAgcDriverModuleId, 0, nullptr, 0, &unloadArgResult) == kModuleUnloaded);
    Require(unloadArgResult == -1);
    int loadArgResult = -1;
    Require(sceSysmoduleLoadModuleInternalWithArg(kAgcDriverModuleId, 0, nullptr, 0, &loadArgResult) == 0);
    Require(loadArgResult == 0);
    Require(sceSysmoduleIsLoadedInternal(kAgcDriverModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternalWithArg(kAgcDriverModuleId, 0, nullptr, 0, &unloadArgResult) == 0);
    Require(unloadArgResult == 0);
    Require(sceSysmoduleIsLoadedInternal(kAgcDriverModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoadedInternal(kAgcModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModuleInternal(kAgcModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModuleInternal(kAgcModuleId) == 0);
    Require(sceSysmoduleIsLoadedInternal(kAgcModuleId) == 0);
    Require(sceSysmoduleLoadModuleInternal(kAgcModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kAgcModuleId) == 0);
    Require(sceSysmoduleIsLoadedInternal(kAgcModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kAgcModuleId) == 0);
    Require(sceSysmoduleIsLoadedInternal(kAgcModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModuleInternal(kAgcModuleId) == kModuleUnloaded);
    CheckUnwindInfo();

    std::int32_t handle = -1;
    Require(sceSysmoduleGetModuleHandleInternal(kFiberModuleId, &handle) == kModuleUnloaded);
    Require(handle == -1);
    Require(ThrowsRuntimeError([] { sceSysmoduleGetModuleHandleInternal(0, nullptr); }));
    Require(ThrowsRuntimeError([&] { sceSysmoduleGetModuleHandleInternal(0, &handle); }));
    Require(ThrowsRuntimeError([] { sceSysmoduleGetModuleHandleInternal(kFiberModuleId, nullptr); }));
    Require(ThrowsRuntimeError([&] { sceSysmoduleGetModuleHandleInternal(0x00ffffffu, &handle); }));

    Require(sceSysmoduleLoadModule(kFiberModuleId) == 0);
    Require(sceSysmoduleGetModuleHandleInternal(kFiberModuleId, &handle) == 0);
    Require(handle >= 0);
    std::int32_t again = -1;
    Require(sceSysmoduleGetModuleHandleInternal(kFiberModuleId, &again) == 0);
    Require(again == handle);
    void* symbol = nullptr;
    Require(sceKernelDlsym(handle, "sceFiberOptParamInitialize", &symbol) == 0);
    Require(symbol != nullptr);
    Require(sceSysmoduleUnloadModule(kFiberModuleId) == 0);
    Require(sceSysmoduleGetModuleHandleInternal(kFiberModuleId, &handle) == kModuleUnloaded);

    constexpr std::uint16_t kAudioPropagationModuleId = 0x0120;
    std::int32_t audioPropagationHandle = -1;
    Require(sceSysmoduleGetModuleHandleInternal(kAudioPropagationModuleId, &audioPropagationHandle) == kModuleUnloaded);
    Require(audioPropagationHandle == -1);

    Require(sceSysmoduleIsLoaded(kAudioPropagationModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kAudioPropagationModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kAudioPropagationModuleId) == 0);
    Require(sceSysmoduleGetModuleHandleInternal(kAudioPropagationModuleId, &audioPropagationHandle) == 0);
    Require(audioPropagationHandle >= 0);
    void* audioPropagationSymbol = nullptr;
    Require(sceKernelDlsym(audioPropagationHandle, "sceAudioPropagationSystemCreate", &audioPropagationSymbol) == 0);
    Require(audioPropagationSymbol != nullptr);

    Require(sceSysmoduleUnloadModule(kAudioPropagationModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kAudioPropagationModuleId) == kModuleUnloaded);
    Require(sceSysmoduleGetModuleHandleInternal(kAudioPropagationModuleId, &audioPropagationHandle) == kModuleUnloaded);
}
