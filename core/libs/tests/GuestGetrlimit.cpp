#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

struct ResourceLimit {
    std::int64_t rlim_cur;
    std::int64_t rlim_max;
};

extern "C" int APS5_VABI getrlimit_nid_postfix(int resource, ResourceLimit* limit);
extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI getdtablesize_nid_postfix(void);
extern "C" int APS5_VABI setrlimit_nid_postfix(int resource, const ResourceLimit* limit);
extern "C" int APS5_VABI pipe_nid_postfix(int* descriptors);
extern "C" int APS5_VABI close_nid_postfix(int descriptor);
extern "C" void* APS5_VABI fopen_nid_postfix(const char* filename, const char* mode);
extern "C" int APS5_VABI fclose_nid_postfix(void* stream);

static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_RLIMIT_CPU = 0;
static constexpr int GUEST_RLIMIT_FSIZE = 1;
static constexpr int GUEST_RLIMIT_DATA = 2;
static constexpr int GUEST_RLIMIT_STACK = 3;
static constexpr int GUEST_RLIMIT_CORE = 4;
static constexpr int GUEST_RLIMIT_RSS = 5;
static constexpr int GUEST_RLIMIT_MEMLOCK = 6;
static constexpr int GUEST_RLIMIT_NPROC = 7;
static constexpr int GUEST_RLIMIT_NOFILE = 8;
static constexpr int GUEST_RLIMIT_SBSIZE = 9;
static constexpr int GUEST_RLIMIT_AS = 10;
static constexpr int GUEST_RLIMIT_NPTS = 11;
static constexpr int GUEST_RLIMIT_SWAP = 12;
static constexpr int GUEST_RLIMIT_KQUEUES = 13;
static constexpr int GUEST_RLIMIT_UMTXP = 14;
static constexpr int GUEST_RLIM_NLIMITS = 15;
static constexpr std::int64_t GUEST_RLIM_INFINITY = std::numeric_limits<std::int64_t>::max();

static void Require(bool value) { if (!value) std::abort(); }

static ResourceLimit GuestLimit(int resource) {
    ResourceLimit limit{-1, -1};
    Require(getrlimit_nid_postfix(resource, &limit) == 0);
    Require(limit.rlim_cur >= 0 && limit.rlim_cur <= limit.rlim_max);
    return limit;
}

#if defined(_WIN32) || defined(__APPLE__)
static void RequireDescriptorsEndAt(const ResourceLimit& limit) {
    constexpr int GUEST_EMFILE = 24;
    std::vector<int> descriptors;
    int pair[2]{-1, -1};
    *__error_nid_postfix() = 0;
    while (pipe_nid_postfix(pair) == 0) {
        Require(pair[0] >= 0 && pair[0] < limit.rlim_cur && pair[1] >= 0 && pair[1] < limit.rlim_cur);
        descriptors.push_back(pair[0]);
        descriptors.push_back(pair[1]);
        Require(descriptors.size() <= static_cast<std::size_t>(limit.rlim_cur));
    }
    Require(*__error_nid_postfix() == GUEST_EMFILE);
    Require(descriptors.size() + 64 >= static_cast<std::size_t>(limit.rlim_cur));
    for (const int descriptor : descriptors) Require(close_nid_postfix(descriptor) == 0);
}
#endif

#ifdef _WIN32
static void RequireStreamsOpen() {
    const auto path = (std::filesystem::temp_directory_path() / ("aps5-open-streams-" + std::to_string(GetCurrentProcessId()))).string();
    std::vector<void*> streams;
    for (int index = 0; index < 1024; ++index) {
        void* stream = fopen_nid_postfix(path.c_str(), "ab");
        Require(stream != nullptr);
        streams.push_back(stream);
    }
    for (void* stream : streams) Require(fclose_nid_postfix(stream) == 0);
    std::filesystem::remove(path);
}
#endif

static void CheckOpenFileLimit() {
    const ResourceLimit limit = GuestLimit(GUEST_RLIMIT_NOFILE);
    Require(limit.rlim_cur == limit.rlim_max);
#ifdef _WIN32
    Require(limit.rlim_cur == 8192);
    RequireDescriptorsEndAt(limit);
    RequireStreamsOpen();
#else
    rlimit host{};
    Require(getrlimit(RLIMIT_NOFILE, &host) == 0);
    Require(host.rlim_cur == host.rlim_max);
    Require(host.rlim_cur == RLIM_INFINITY ? limit.rlim_cur == GUEST_RLIM_INFINITY : limit.rlim_cur == static_cast<std::int64_t>(host.rlim_cur));
#endif
#ifdef __APPLE__
    int perProcess = 0;
    std::size_t size = sizeof(perProcess);
    Require(sysctlbyname("kern.maxfilesperproc", &perProcess, &size, nullptr, 0) == 0);
    Require(perProcess > 0 && limit.rlim_cur <= perProcess);
    RequireDescriptorsEndAt(limit);
#endif
    Require(setrlimit_nid_postfix(GUEST_RLIMIT_NOFILE, &limit) == 0);
    const ResourceLimit lowered{limit.rlim_max > 1 ? limit.rlim_max - 1 : 1, limit.rlim_max};
    Require(setrlimit_nid_postfix(GUEST_RLIMIT_NOFILE, &lowered) == 0);
    const ResourceLimit updated = GuestLimit(GUEST_RLIMIT_NOFILE);
    Require(updated.rlim_cur == lowered.rlim_cur);
    Require(updated.rlim_max == lowered.rlim_max);
    Require(setrlimit_nid_postfix(GUEST_RLIMIT_NOFILE, &limit) == 0);
    const ResourceLimit restored = GuestLimit(GUEST_RLIMIT_NOFILE);
    Require(restored.rlim_cur == limit.rlim_cur);
    Require(restored.rlim_max == limit.rlim_max);
    if (limit.rlim_max != GUEST_RLIM_INFINITY) {
        const ResourceLimit raised{limit.rlim_cur, limit.rlim_max + 1};
        bool raisedRejected = false;
        try {
            setrlimit_nid_postfix(GUEST_RLIMIT_NOFILE, &raised);
        } catch (const std::runtime_error&) {
            raisedRejected = true;
        }
        Require(raisedRejected);
    }
}

static void RequireError(int resource, ResourceLimit* limit, int error) {
    *__error_nid_postfix() = 0;
    Require(getrlimit_nid_postfix(resource, limit) == -1);
    Require(*__error_nid_postfix() == error);
}

static void RequireInvalidOutputPointer(int resource) {
    auto* const invalid = reinterpret_cast<ResourceLimit*>(static_cast<std::uintptr_t>(1));
    RequireError(resource, invalid, GUEST_EFAULT);
}

#ifndef _WIN32
static std::int64_t Guest(rlim_t value) {
    return value == RLIM_INFINITY ? GUEST_RLIM_INFINITY : static_cast<std::int64_t>(value);
}

static void RequireHostLimit(int resource, int hostResource) {
    rlimit host{};
    Require(getrlimit(hostResource, &host) == 0);
    const ResourceLimit guest = GuestLimit(resource);
    Require(guest.rlim_cur == Guest(host.rlim_cur));
    Require(guest.rlim_max == Guest(host.rlim_max));
}

static void LoweredHostLimitIsReported() {
    rlimit host{};
    Require(getrlimit(RLIMIT_DATA, &host) == 0);
    const rlim_t lowered = rlim_t{1} << 40;
    if (host.rlim_max != RLIM_INFINITY && host.rlim_max < lowered) return;
    host.rlim_cur = lowered;
    Require(setrlimit(RLIMIT_DATA, &host) == 0);
    Require(GuestLimit(GUEST_RLIMIT_DATA).rlim_cur == static_cast<std::int64_t>(lowered));
}
#endif

int main() {
#ifdef _WIN32
    GuestLimit(GUEST_RLIMIT_DATA);
#else
    RequireHostLimit(GUEST_RLIMIT_DATA, RLIMIT_DATA);
    LoweredHostLimitIsReported();
#endif
    for (int resource : {
             GUEST_RLIMIT_CPU,
             GUEST_RLIMIT_FSIZE,
             GUEST_RLIMIT_CORE,
             GUEST_RLIMIT_RSS,
             GUEST_RLIMIT_MEMLOCK,
             GUEST_RLIMIT_SBSIZE,
             GUEST_RLIMIT_AS,
             GUEST_RLIMIT_NPTS,
             GUEST_RLIMIT_SWAP,
             GUEST_RLIMIT_KQUEUES,
             GUEST_RLIMIT_UMTXP,
         }) {
        const ResourceLimit guest = GuestLimit(resource);
        Require(guest.rlim_cur == GUEST_RLIM_INFINITY);
        Require(guest.rlim_max == GUEST_RLIM_INFINITY);
        RequireError(resource, nullptr, GUEST_EFAULT);
    }
    ResourceLimit limit{};
    RequireError(GUEST_RLIM_NLIMITS, &limit, GUEST_EINVAL);
    RequireError(-1, &limit, GUEST_EINVAL);
    RequireError(GUEST_RLIM_NLIMITS, nullptr, GUEST_EINVAL);
    RequireError(-1, nullptr, GUEST_EINVAL);
    RequireError(GUEST_RLIMIT_DATA, nullptr, GUEST_EFAULT);
    RequireInvalidOutputPointer(GUEST_RLIMIT_DATA);
    RequireInvalidOutputPointer(GUEST_RLIMIT_NOFILE);
    const ResourceLimit stack = GuestLimit(GUEST_RLIMIT_STACK);
    Require(stack.rlim_cur == 0x200000 && stack.rlim_max == 0x200000);
    RequireError(GUEST_RLIMIT_STACK, nullptr, GUEST_EFAULT);
    const ResourceLimit processes = GuestLimit(GUEST_RLIMIT_NPROC);
    Require(processes.rlim_cur == 1604 && processes.rlim_max == 1604);
    RequireError(GUEST_RLIMIT_NPROC, nullptr, GUEST_EFAULT);
    const ResourceLimit nofile = GuestLimit(GUEST_RLIMIT_NOFILE);
    const int tableSize = getdtablesize_nid_postfix();
    std::int64_t expectedTableSize = nofile.rlim_cur;
#ifndef _WIN32
    expectedTableSize = std::min(expectedTableSize, static_cast<std::int64_t>(::getdtablesize()));
#endif
    if (expectedTableSize > std::numeric_limits<int>::max())
        expectedTableSize = std::numeric_limits<int>::max();
    Require(tableSize == static_cast<int>(expectedTableSize));
    CheckOpenFileLimit();
}
