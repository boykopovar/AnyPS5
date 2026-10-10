#include "prx/libc/include/CpuTopology.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct CpuListCase {
    std::string_view text;
    std::uint64_t mask;
    std::string_view description;
};

std::uint64_t Parse(std::string_view text) {
    const Testing::TemporaryDirectory directory;
    const auto path = directory.Path() / "cpus";
    {
        std::ofstream file(path, std::ios::trunc);
        file << text;
        Require(static_cast<bool>(file), "write the cpu list file");
    }
    return CpuTopology::MaskFromCpuList(path.c_str());
}

void RequireMasks(std::initializer_list<CpuListCase> cases) {
    for (const auto& entry : cases) {
        RequireEqual(Parse(entry.text), entry.mask, std::string(entry.description));
    }
}

class ThreadAffinity {
public:
    ThreadAffinity() {
        CPU_ZERO(&original);
        Require(pthread_getaffinity_np(pthread_self(), sizeof(original), &original) == 0, "read the thread affinity");
    }

    ~ThreadAffinity() {
        pthread_setaffinity_np(pthread_self(), sizeof(original), &original);
    }

    ThreadAffinity(const ThreadAffinity&) = delete;
    ThreadAffinity& operator=(const ThreadAffinity&) = delete;

private:
    cpu_set_t original;
};

std::uint64_t FirstProcessCpu() {
    const auto process = CpuTopology::Get().process;
    return process & (~process + 1);
}

const Case validLists{"MaskFromCpuList_ValidLists_ReturnsCpuMask", [] {
    RequireMasks({
        {"0-15\n", 0xffffull, "range"},
        {"16-31\n", 0xffff0000ull, "high range"},
        {"0,2,4-5\n", 0x35ull, "list of cpus and ranges"},
        {"3", 0x8ull, "single cpu without a newline"},
        {"62-70\n", 0xc000000000000000ull, "cpus past 63 are left out"},
    });
}};

const Case invalidLists{"MaskFromCpuList_EmptyOrMalformedLists_ReturnsZero", [] {
    RequireMasks({
        {"", 0, "empty list"},
        {"4-2\n", 0, "reversed range"},
        {"0-x\n", 0, "malformed range"},
        {"0;1\n", 0, "malformed separator"},
    });
}};

const Case missingFile{"MaskFromCpuList_MissingFile_ReturnsZero", [] {
    RequireEqual(CpuTopology::MaskFromCpuList("/nonexistent/anyps5/cpus"), std::uint64_t{0}, "missing file");
}};

const Case layout{"CpuTopologyGet_CurrentProcess_ReadsAffinityAndSeparatesHybridClasses", [] {
    const auto& current = CpuTopology::Get();
    Require(current.process != 0, "process affinity read");
    Require(!current.hybrid || (current.performant & current.efficient) == 0, "hybrid classes overlap");
}};

const Case pinCurrent{"Pin_CurrentThreadToOneProcessCpu_RestrictsThreadAffinity", [] {
    const ThreadAffinity restore;
    const std::uint64_t first = FirstProcessCpu();
    RequireEqual(CpuTopology::Pin(nullptr, first), first, "pin to one cpu of the process");
    cpu_set_t set;
    CPU_ZERO(&set);
    Require(pthread_getaffinity_np(pthread_self(), sizeof(set), &set) == 0, "read thread affinity");
    RequireEqual(CPU_COUNT(&set), 1, "cpus in the thread affinity");
    Require(CPU_ISSET(static_cast<unsigned>(__builtin_ctzll(first)), &set), "thread runs on the pinned cpu");
}};

const Case pinOutside{"Pin_MaskOutsideProcess_IsNotApplied", [] {
    const ThreadAffinity restore;
    RequireEqual(CpuTopology::Pin(nullptr, ~CpuTopology::Get().process), std::uint64_t{0}, "mask outside the process");
}};

const Case pinOtherThread{"Pin_OtherThreadHandle_IsNotApplied", [] {
    const ThreadAffinity restore;
    int dummy = 0;
    RequireEqual(CpuTopology::Pin(&dummy, FirstProcessCpu()), std::uint64_t{0}, "other threads are not pinned by handle");
}};

} // namespace
