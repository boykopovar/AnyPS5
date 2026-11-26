#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uidna.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using IcuRecoveredTest::Require;
using ConvertFunction = decltype(&uidna_nameToASCII_nid_postfix);

int RejectUnreadableInput(ConvertFunction convert) {
    alarm(5);
    const rlimit noCore{0, 0};
    Require(setrlimit(RLIMIT_CORE, &noCore) == 0, "disable child core dumps");
    std::int32_t status = 0;
    auto* handle = uidna_openUTS46_nid_postfix(0, &status);
    Require(handle != nullptr && status == 0, "create input-allocation test handle");
    const auto pageSize = sysconf(_SC_PAGESIZE);
    Require(pageSize > 0, "query input guard page size");
    const auto page = static_cast<std::size_t>(pageSize);
    auto* mapping = static_cast<std::uint8_t*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(mapping != MAP_FAILED, "map bounded input guard");
    Require(mprotect(mapping + page, page, PROT_NONE) == 0, "protect second input page");
    auto* input = reinterpret_cast<char16_t*>(mapping + page - sizeof(char16_t));
    *input = u'a';
    alignas(UIDNAInfo) std::array<std::uint8_t, 32> info;
    info.fill(0xa5);
    const std::int16_t infoSize = 16;
    std::memcpy(info.data(), &infoSize, sizeof(infoSize));
    const auto expectedInfo = info;
    auto* statm = std::fopen("/proc/self/statm", "r");
    Require(statm != nullptr, "open child virtual size");
    unsigned long long pages = 0;
    Require(std::fscanf(statm, "%llu", &pages) == 1, "read child virtual size");
    Require(std::fclose(statm) == 0, "close child virtual size");
    constexpr rlim_t headroom = rlim_t{8} << 20;
    Require(pages <= (std::numeric_limits<rlim_t>::max() - headroom) / page,
        "child virtual size fits resource limit");
    rlimit original{};
    Require(getrlimit(RLIMIT_AS, &original) == 0, "read child address-space limit");
    auto bounded = original;
    const auto current = static_cast<rlim_t>(pages) * page;
    bounded.rlim_cur = std::min(original.rlim_cur, current + headroom);
    Require(bounded.rlim_cur > current, "child limit leaves reporting headroom");
    Require(setrlimit(RLIMIT_AS, &bounded) == 0, "bound child address space before invalid input");
    rlimit actual{};
    Require(getrlimit(RLIMIT_AS, &actual) == 0 && actual.rlim_cur == bounded.rlim_cur,
        "child address-space limit is active");
    const auto before = NotImplementedCalls_nid_no_patch();
    bool refused = false;
    std::int32_t result = -1;
    try {
        result = convert(handle, input, 16 * 1024 * 1024, nullptr, 0,
            reinterpret_cast<UIDNAInfo*>(info.data()), &status);
    } catch (const std::runtime_error&) {
        refused = NotImplementedCalls_nid_no_patch() == before + 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "input allocation regression: unexpected exception: %s\n", error.what());
    }
    const bool intact = status == 0 && info == expectedInfo && *input == u'a';
    if (!refused || !intact) {
        std::fprintf(stderr, "input allocation regression: expected controlled unreadable-input refusal before full allocation; result=%d status=%d refused=%d intact=%d\n",
            result, status, refused, intact);
    }
    uidna_close_nid_postfix(handle);
    Require(munmap(mapping, 2 * page) == 0, "release bounded input guard");
    return refused && intact ? 0 : 1;
}

}

void CheckIcuIdnaInputAllocation() {
    bool passed = true;
    for (auto convert : {uidna_nameToASCII_nid_postfix, uidna_nameToUnicode_nid_postfix}) {
        const auto child = fork();
        Require(child >= 0, "start bounded input-allocation subprocess");
        if (child == 0) _exit(RejectUnreadableInput(convert));
        int status = 0;
        Require(waitpid(child, &status, 0) == child, "wait for input-allocation subprocess");
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::fprintf(stderr, "input allocation subprocess failed: wait status=%d\n", status);
            passed = false;
        }
    }
    Require(passed, "both conversions reject inaccessible explicit input before allocating its declared length");
}
