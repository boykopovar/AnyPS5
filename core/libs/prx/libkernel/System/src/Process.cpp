#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sched.h>
#include <unistd.h>
#endif

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifndef _WIN32
#include <sys/resource.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

constexpr int sceInvalidArgument = static_cast<int>(0x80020016u);
constexpr int errnoNoChild = 10;
constexpr int errnoInvalidArgument = 22;
constexpr unsigned freebsdWaitOptions = 0x8000003Fu;

std::atomic<std::uint32_t> gpoBits{0};
constexpr std::array<std::uint8_t, 16> openPsId{'A', 'n', 'y', 'P', 'S', '5', 'O', 'p', 'e', 'n', 'P', 's', 'I', 'd', 0, 1};

class ProcessArguments {
public:
    ProcessArguments() {
#ifdef _WIN32
        std::array<char, 32768> path{};
        const auto size = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (size == 0)
            throw std::system_error(GetLastError(), std::system_category(), "Reading executable path");
        if (size >= path.size())
            throw std::runtime_error("Executable path exceeds the guest argument buffer");
        arguments.emplace_back(path.data(), size);
#else
        std::ifstream stream("/proc/self/cmdline", std::ios::binary);
        if (!stream)
            throw std::runtime_error("Cannot read process arguments");
        std::string argument;
        while (std::getline(stream, argument, '\0')) {
            if (stream.eof())
                throw std::runtime_error("Unterminated process argument");
            arguments.push_back(argument);
        }
        if (stream.bad() || !stream.eof())
            throw std::runtime_error("Reading process arguments failed");
#endif
        if (arguments.empty() || arguments.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Invalid process argument count");
        for (const auto& argument : arguments)
            pointers.push_back(argument.c_str());
        pointers.push_back(nullptr);
    }

    int GetCount() const { return static_cast<int>(arguments.size()); }
    const char** GetValues() { return pointers.data(); }

private:
    std::vector<std::string> arguments;
    std::vector<const char*> pointers;
};

ProcessArguments& getProcessArguments() {
    static ProcessArguments arguments;
    return arguments;
}

void validateSchedulingPolicy(int policy) {
    if (policy < 1 || policy > 3)
        throw std::invalid_argument("Unsupported guest scheduling policy");
}

constexpr int guestFault = 14;
constexpr int guestInvalid = 22;
constexpr int guestRlimitData = 2;
constexpr int guestRlimitCount = 15;
constexpr std::int64_t guestRlimitInfinity = std::numeric_limits<std::int64_t>::max();

#ifdef _WIN32
std::int64_t hostMemoryLimit() {
    BOOL inJob = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &inJob))
        throw std::system_error(GetLastError(), std::system_category(), "getrlimit: IsProcessInJob failed");
    if (!inJob)
        return guestRlimitInfinity;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    if (!QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation, &limits, sizeof(limits), nullptr))
        throw std::system_error(GetLastError(), std::system_category(), "getrlimit: QueryInformationJobObject failed");
    if ((limits.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_PROCESS_MEMORY) == 0 || limits.ProcessMemoryLimit > static_cast<SIZE_T>(guestRlimitInfinity))
        return guestRlimitInfinity;
    return static_cast<std::int64_t>(limits.ProcessMemoryLimit);
}
#else
std::int64_t toGuestLimit(rlim_t value) {
    if (value == RLIM_INFINITY || value > static_cast<rlim_t>(guestRlimitInfinity))
        return guestRlimitInfinity;
    return static_cast<std::int64_t>(value);
}
#endif

}

extern "C" int* APS5_VABI __error_nid_postfix();

struct GuestResourceLimit {
    std::int64_t rlim_cur;
    std::int64_t rlim_max;
};

struct GuestResourceUsage {
    KernelTimeval ru_utime;
    KernelTimeval ru_stime;
    std::int64_t ru_maxrss;
    std::int64_t ru_ixrss;
    std::int64_t ru_idrss;
    std::int64_t ru_isrss;
    std::int64_t ru_minflt;
    std::int64_t ru_majflt;
    std::int64_t ru_nswap;
    std::int64_t ru_inblock;
    std::int64_t ru_oublock;
    std::int64_t ru_msgsnd;
    std::int64_t ru_msgrcv;
    std::int64_t ru_nsignals;
    std::int64_t ru_nvcsw;
    std::int64_t ru_nivcsw;
};

extern "C" Pthread APS5_VABI scePthreadSelf();

extern "C" {

// unknown data
const char* __progname_nid_postfix = "eboot.bin";
static char* emptyEnvironment[1];
char** environ_nid_postfix = emptyEnvironment;

int APS5_VABI getargc_nid_postfix(void) {
    return getProcessArguments().GetCount();
}

const char** APS5_VABI getargv_nid_postfix(void) {
    return getProcessArguments().GetValues();
}

int APS5_VABI getpagesize_nid_postfix(void) {
    return PS5_PAGE_SIZE;
}

int APS5_VABI getpid_nid_postfix(void) {
#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = ::getpid();
#endif
    if (pid == 0 || static_cast<std::uint64_t>(pid) > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Process identifier is outside the guest range");
    return static_cast<int>(pid);
}

int APS5_VABI getuid_nid_postfix(void) {
    return 0;
}

int APS5_VABI geteuid_nid_postfix(void) {
    return 0;
}

int APS5_VABI getgid_nid_postfix(void) {
    return 0;
}

int APS5_VABI getegid_nid_postfix(void) {
    return 0;
}

int APS5_VABI issetugid_nid_postfix(void) {
    return 0;
}

void APS5_VABI exit_nid_postfix(int code) {
    LibcExit_nid_no_patch(code);
}

[[noreturn]] void APS5_VABI _exit_nid_postfix(int status) {
    std::_Exit(status);
}

int APS5_VABI system_nid_postfix(const char* command) {
    constexpr int shellNotExecuted = 127 << 8;
    return command == nullptr ? 1 : shellNotExecuted;
}

int APS5_VABI waitpid_nid_postfix(int pid, int* status, int options) {
    (void)pid;
    (void)status;
    *__error_nid_postfix() = (static_cast<unsigned>(options) & ~freebsdWaitOptions) != 0 ? errnoInvalidArgument : errnoNoChild;
    return -1;
}

int APS5_VABI sceKernelGetCurrentCpu(void) {
#ifdef _WIN32
    PROCESSOR_NUMBER processor{};
    GetCurrentProcessorNumberEx(&processor);
    unsigned index = processor.Number;
    for (WORD group = 0; group < processor.Group; ++group) {
        const auto count = GetActiveProcessorCount(group);
        if (count == 0)
            throw std::system_error(GetLastError(), std::system_category(), "Reading processor group size");
        index += count;
    }
#else
    const int cpu = ::sched_getcpu();
    if (cpu < 0)
        throw std::system_error(errno, std::generic_category(), "Reading current processor");
    const auto index = static_cast<unsigned>(cpu);
#endif
    return GuestCpuFromHost(index, scePthreadSelf()->affinity.load(std::memory_order_relaxed));
}

std::uint64_t APS5_VABI sceKernelGetGPI(void) {
    return gpoBits.load(std::memory_order_relaxed);
}

void APS5_VABI sceKernelSetGPO(std::uint32_t bits) {
    gpoBits.store(bits, std::memory_order_relaxed);
}

int APS5_VABI sceKernelGetOpenPsId(void* openPsIdOutput) {
    if (!openPsIdOutput)
        throw std::invalid_argument("sceKernelGetOpenPsId: null output");
    std::memcpy(openPsIdOutput, openPsId.data(), openPsId.size());
    return 0;
}

void* APS5_VABI sceKernelGetProcParam(void) {
    return const_cast<void*>(ApplicationProcessParameters_nid_no_patch());
}

int APS5_VABI sceKernelUuidCreate(std::uint32_t* uuid) {
    if (!uuid) return sceInvalidArgument;
    static thread_local std::random_device device;
    std::uniform_int_distribution<std::uint32_t> distribution;
    std::array<std::uint32_t, 4> value;
    for (auto& word : value)
        word = distribution(device);
    value[1] = (value[1] & 0x0fffffffu) | 0x40000000u;
    value[2] = (value[2] & 0xffffff3fu) | 0x80u;
    std::memcpy(uuid, value.data(), sizeof(value));
    return 0;
}

void APS5_VABI sceKernelSync(void) {
    SyncWrittenPaths_nid_no_patch();
}

int APS5_VABI sched_get_priority_max_nid_postfix(int policy) {
    validateSchedulingPolicy(policy);
    return 256;
}

int APS5_VABI sched_get_priority_min_nid_postfix(int policy) {
    validateSchedulingPolicy(policy);
    return 767;
}

int APS5_VABI getrusage_nid_postfix(int who, GuestResourceUsage* usage) {
    if (usage == nullptr)
        throw std::invalid_argument("getrusage: usage is null");
    if (who != 0 && who != 1)
        throw std::invalid_argument("getrusage: unsupported who");
#ifdef _WIN32
    FILETIME creation{};
    FILETIME exitTime{};
    FILETIME kernel{};
    FILETIME user{};
    if (who == 0) {
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exitTime, &kernel, &user))
            throw std::system_error(GetLastError(), std::system_category(), "getrusage: GetProcessTimes failed");
    } else {
        if (!GetThreadTimes(GetCurrentThread(), &creation, &exitTime, &kernel, &user))
            throw std::system_error(GetLastError(), std::system_category(), "getrusage: GetThreadTimes failed");
    }
    const auto toMicros = [](const FILETIME& time) {
        return ((static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime) / 10ULL;
    };
    const auto userMicros = toMicros(user);
    const auto kernelMicros = toMicros(kernel);
    usage->ru_utime.tv_sec = static_cast<std::int64_t>(userMicros / 1000000ULL);
    usage->ru_utime.tv_usec = static_cast<std::int64_t>(userMicros % 1000000ULL);
    usage->ru_stime.tv_sec = static_cast<std::int64_t>(kernelMicros / 1000000ULL);
    usage->ru_stime.tv_usec = static_cast<std::int64_t>(kernelMicros % 1000000ULL);
    usage->ru_maxrss = 0;
    usage->ru_ixrss = 0;
    usage->ru_idrss = 0;
    usage->ru_isrss = 0;
    usage->ru_minflt = 0;
    usage->ru_majflt = 0;
    usage->ru_nswap = 0;
    usage->ru_inblock = 0;
    usage->ru_oublock = 0;
    usage->ru_msgsnd = 0;
    usage->ru_msgrcv = 0;
    usage->ru_nsignals = 0;
    usage->ru_nvcsw = 0;
    usage->ru_nivcsw = 0;
#else
    struct rusage native{};
    if (::getrusage(who == 0 ? RUSAGE_SELF : RUSAGE_THREAD, &native) != 0)
        throw std::system_error(errno, std::generic_category(), "getrusage: getrusage failed");
    usage->ru_utime.tv_sec = static_cast<std::int64_t>(native.ru_utime.tv_sec);
    usage->ru_utime.tv_usec = static_cast<std::int64_t>(native.ru_utime.tv_usec);
    usage->ru_stime.tv_sec = static_cast<std::int64_t>(native.ru_stime.tv_sec);
    usage->ru_stime.tv_usec = static_cast<std::int64_t>(native.ru_stime.tv_usec);
    usage->ru_maxrss = static_cast<std::int64_t>(native.ru_maxrss);
    usage->ru_ixrss = static_cast<std::int64_t>(native.ru_ixrss);
    usage->ru_idrss = static_cast<std::int64_t>(native.ru_idrss);
    usage->ru_isrss = static_cast<std::int64_t>(native.ru_isrss);
    usage->ru_minflt = static_cast<std::int64_t>(native.ru_minflt);
    usage->ru_majflt = static_cast<std::int64_t>(native.ru_majflt);
    usage->ru_nswap = static_cast<std::int64_t>(native.ru_nswap);
    usage->ru_inblock = static_cast<std::int64_t>(native.ru_inblock);
    usage->ru_oublock = static_cast<std::int64_t>(native.ru_oublock);
    usage->ru_msgsnd = static_cast<std::int64_t>(native.ru_msgsnd);
    usage->ru_msgrcv = static_cast<std::int64_t>(native.ru_msgrcv);
    usage->ru_nsignals = static_cast<std::int64_t>(native.ru_nsignals);
    usage->ru_nvcsw = static_cast<std::int64_t>(native.ru_nvcsw);
    usage->ru_nivcsw = static_cast<std::int64_t>(native.ru_nivcsw);
#endif
    return 0;
}

int APS5_VABI getrlimit_nid_postfix(int resource, GuestResourceLimit* limit) {
    if (resource < 0 || resource >= guestRlimitCount) {
        *__error_nid_postfix() = guestInvalid;
        return -1;
    }
    if (limit == nullptr) {
        *__error_nid_postfix() = guestFault;
        return -1;
    }
    if (resource != guestRlimitData)
        throw std::runtime_error("getrlimit: unsupported resource " + std::to_string(resource));
#ifdef _WIN32
    const std::int64_t memory = hostMemoryLimit();
    limit->rlim_cur = memory;
    limit->rlim_max = memory;
#else
    rlimit native{};
    if (::getrlimit(RLIMIT_DATA, &native) != 0)
        throw std::system_error(errno, std::generic_category(), "getrlimit: getrlimit failed");
    limit->rlim_cur = toGuestLimit(native.rlim_cur);
    limit->rlim_max = toGuestLimit(native.rlim_max);
#endif
    return 0;
}

int APS5_VABI sceKernelIsTrinityMode(void) {
    return 0;
}

int APS5_VABI sceKernelGetOperationMode(int* mode, int* submode) {
    if (!mode || !submode)
        throw std::invalid_argument("sceKernelGetOperationMode: null output");
    *mode = 0;
    *submode = 0;
    return 0;
}

int APS5_VABI seteuid_nid_postfix(std::uint32_t euid) {
    if (euid != 0)
        throw std::runtime_error("seteuid: unsupported user id " + std::to_string(euid));
    return 0;
}

int APS5_VABI setegid_nid_postfix(std::uint32_t egid) {
    if (egid != 0)
        throw std::runtime_error("setegid: unsupported group id " + std::to_string(egid));
    return 0;
}

}
