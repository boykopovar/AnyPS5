#include "prx/libc/include/general/VabiMacros.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

template<typename TValue>
using Fetch = TValue (APS5_VABI*)(volatile TValue*, TValue);

template<typename TValue>
static TValue Expected(TValue before, TValue operand, const std::string& operation) {
    if (operation == "and") return before & operand;
    if (operation == "or") return before | operand;
    return before ^ operand;
}

template<typename TValue>
static void Check(Fetch<TValue> fetch, const std::string& operation) {
    constexpr TValue maximum = std::numeric_limits<TValue>::max();
    std::array<TValue, 3> guarded{maximum, 0, maximum};
    const auto one = [&](TValue before, TValue operand) {
        guarded[1] = before;
        Require(fetch(&guarded[1], operand) == before, "fetch must return the original value");
        Require(guarded[1] == Expected(before, operand, operation), "bitwise result");
        Require(guarded[0] == maximum && guarded[2] == maximum, "neighboring storage changed");
    };
    if constexpr (sizeof(TValue) == 1) {
        for (unsigned before = 0; before < 256; ++before)
            for (unsigned operand = 0; operand < 256; ++operand)
                one(static_cast<TValue>(before), static_cast<TValue>(operand));
    } else {
        constexpr std::array<TValue, 6> values{0, 1, maximum, static_cast<TValue>(maximum >> 1), static_cast<TValue>(TValue{1} << (sizeof(TValue) * 8 - 1)), static_cast<TValue>(0xA55AA55A)};
        for (TValue before : values) for (TValue operand : values) one(before, operand);
        std::uint32_t random = 0x31415926;
        for (unsigned i = 0; i < 4096; ++i) {
            random = random * 1664525u + 1013904223u;
            const TValue before = static_cast<TValue>(random);
            random = random * 1664525u + 1013904223u;
            one(before, static_cast<TValue>(random));
        }
    }
    guarded[1] = operation == "and" ? maximum : 0;
    std::array<std::thread, 4> workers;
    std::barrier start(4);
    for (unsigned index = 0; index < workers.size(); ++index) {
        workers[index] = std::thread([&, index] {
            const TValue bit = static_cast<TValue>(TValue{1} << index);
            const TValue operand = operation == "and" ? static_cast<TValue>(~bit) : bit;
            start.arrive_and_wait();
            for (unsigned iteration = 0; iteration < 1025; ++iteration) {
                const TValue previous = fetch(&guarded[1], operand);
                const bool expected = operation == "and" ? iteration == 0 : operation == "or" ? iteration != 0 : iteration % 2 != 0;
                Require(((previous & bit) != 0) == expected, "concurrent original-bit history");
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(guarded[1] == (operation == "and" ? static_cast<TValue>(maximum & ~TValue{15}) : TValue{15}), "concurrent updates lost");
    Require(guarded[0] == maximum && guarded[2] == maximum, "concurrent neighboring storage changed");
    std::array<TValue, 2> flags{};
    std::array<TValue, 2> observed{};
    std::barrier phase(3);
    const auto litmus = [&](unsigned index) {
        for (unsigned i = 0; i < 512; ++i) {
            phase.arrive_and_wait();
            fetch(&flags[index], operation == "and" ? TValue{0} : TValue{1});
            observed[index] = fetch(&flags[1 - index], operation == "and" ? maximum : TValue{0});
            phase.arrive_and_wait();
        }
    };
    std::thread first(litmus, 0), second(litmus, 1);
    for (unsigned i = 0; i < 512; ++i) {
        flags.fill(operation == "and" ? maximum : TValue{0});
        phase.arrive_and_wait();
        phase.arrive_and_wait();
        const TValue initial = operation == "and" ? maximum : TValue{0};
        Require(observed[0] != initial || observed[1] != initial, "forbidden sequentially consistent outcome");
    }
    first.join();
    second.join();
}

int main(int argc, char** argv) {
    Require(argc == 5, "library directory, patched or unpatched, operation, width");
    const auto path = std::filesystem::absolute(argv[1]) / "libc.prx";
    const std::string operation = argv[3];
    const int width = std::atoi(argv[4]);
    Require(operation == "and" || operation == "or" || operation == "xor", "operation");
    Require(width == 1 || width == 2 || width == 4, "width");
    const std::string name = "_Fetch_" + operation + "_seq_cst_" + std::to_string(width);
    const std::string symbol = std::string(argv[2]) == "patched" ? Nid::ComputeNid(name, "") : name + "_nid_postfix";
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const auto startup = GetModuleHandleW(L"libc.prx");
    const auto library = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    Require(library != nullptr, "load libc");
    std::array<wchar_t, 32768> loaded{};
    const auto length = GetModuleFileNameW(library, loaded.data(), static_cast<DWORD>(loaded.size()));
    Require(length != 0 && length < loaded.size(), "loaded libc path");
    Require(std::filesystem::equivalent(path, std::filesystem::path(loaded.data())), "wrong libc loaded");
    if (startup) Require(startup == library, "startup libc differs from selected libc");
    std::printf("Selected libc: %ls (startup module %s)\n", loaded.data(), startup ? "same" : "not imported");
    const auto function = reinterpret_cast<void*>(GetProcAddress(library, symbol.c_str()));
#else
    const auto library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr, "load libc");
    const auto function = dlsym(library, symbol.c_str());
#endif
    if (!function) std::fprintf(stderr, "Missing %s (%s)\n", name.c_str(), symbol.c_str());
    Require(function != nullptr, "cannot exercise missing helper");
    if (width == 1) Check(reinterpret_cast<Fetch<std::uint8_t>>(function), operation);
    if (width == 2) Check(reinterpret_cast<Fetch<std::uint16_t>>(function), operation);
    if (width == 4) Check(reinterpret_cast<Fetch<std::uint32_t>>(function), operation);
    std::printf("%s: value, original result, bounds, concurrency and ordering checks passed\n", name.c_str());
}
