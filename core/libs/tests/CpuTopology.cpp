#include "prx/libc/include/CpuTopology.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "cpu topology test failed: %s\n", what);
    std::exit(1);
}

static std::uint64_t Parse(const char* text) {
    const auto path = std::filesystem::temp_directory_path() / "anyps5_cpu_topology_test_cpus";
    {
        std::ofstream file(path, std::ios::trunc);
        file << text;
    }
    const auto mask = CpuTopology::MaskFromCpuList(path.c_str());
    std::filesystem::remove(path);
    return mask;
}

int main() {
    Require(Parse("0-15\n") == 0xffffull, "range");
    Require(Parse("16-31\n") == 0xffff0000ull, "high range");
    Require(Parse("0,2,4-5\n") == 0x35ull, "list of cpus and ranges");
    Require(Parse("3") == 0x8ull, "single cpu without a newline");
    Require(Parse("62-70\n") == 0xc000000000000000ull, "cpus past 63 are left out");
    Require(Parse("") == 0, "empty list");
    Require(Parse("4-2\n") == 0, "reversed range");
    Require(Parse("0-x\n") == 0, "malformed range");
    Require(Parse("0;1\n") == 0, "malformed separator");
    Require(CpuTopology::MaskFromCpuList("/nonexistent/anyps5/cpus") == 0, "missing file");

    const auto& layout = CpuTopology::Get();
    Require(layout.process != 0, "process affinity read");
    Require(!layout.hybrid || (layout.performant & layout.efficient) == 0, "hybrid classes overlap");
    const std::uint64_t first = layout.process & (~layout.process + 1);
    Require(CpuTopology::Pin(nullptr, first) == first, "pin to one cpu of the process");
    cpu_set_t set;
    CPU_ZERO(&set);
    Require(pthread_getaffinity_np(pthread_self(), sizeof(set), &set) == 0, "read thread affinity");
    Require(CPU_COUNT(&set) == 1 && CPU_ISSET(static_cast<unsigned>(__builtin_ctzll(first)), &set), "thread runs on the pinned cpu only");
    Require(CpuTopology::Pin(nullptr, ~layout.process) == 0, "mask outside the process is not applied");
    int dummy = 0;
    Require(CpuTopology::Pin(&dummy, first) == 0, "other threads are not pinned by handle");
    return 0;
}
