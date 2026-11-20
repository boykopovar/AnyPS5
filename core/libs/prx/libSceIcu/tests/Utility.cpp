#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu utility: " + message);
}

constexpr std::int32_t ZeroError = 0;

int tracedEntries = 0;
int tracedExits = 0;
int tracedData = 0;
std::int32_t lastFunction = -1;
const char* lastFormat = nullptr;
bool lastFormatMatched = false;

void APS5_VABI TraceEntry(const void* context, std::int32_t function) {
    Require(context == reinterpret_cast<const void*>(0x1234), "the trace context was not passed");
    lastFunction = function;
    ++tracedEntries;
}

void APS5_VABI TraceExit(const void* context, std::int32_t function, const char* format, ...) {
    (void)context;
    (void)format;
    lastFunction = function;
    ++tracedExits;
}

void APS5_VABI TraceData(const void* context, std::int32_t function, std::int32_t level, const char* format, ...) {
    (void)context;
    (void)level;
    Require(format != nullptr && std::strcmp(format, "value %d") == 0, "the trace format was not forwarded");
    lastFunction = function;
    lastFormat = format;
    lastFormatMatched = true;
    ++tracedData;
}

std::int32_t APS5_VABI CompareInts(const void* context, const void* left, const void* right) {
    (void)context;
    const auto l = *static_cast<const std::int32_t*>(left);
    const auto r = *static_cast<const std::int32_t*>(right);
    return l < r ? -1 : l > r ? 1 : 0;
}

}

int main() {
    char16_t buffer[16] = {};
    Require(uprv_itou_nid_postfix(buffer, 16, 42, 10, 1) == 2, "itou returned the wrong length");
    Require(buffer[0] == 0x34 && buffer[1] == 0x32 && buffer[2] == 0, "itou did not format 42 in decimal");
    Require(uprv_itou_nid_postfix(buffer, 16, 255, 16, 1) == 2, "itou returned the wrong hex length");
    Require(buffer[0] == 0x46 && buffer[1] == 0x46, "itou did not format 255 in hex");
    Require(uprv_itou_nid_postfix(buffer, 16, 7, 10, 4) == 4, "itou ignored the minimum width");
    Require(buffer[0] == 0x30 && buffer[1] == 0x30 && buffer[2] == 0x30 && buffer[3] == 0x37,
        "itou did not zero-pad");

    Require(uprv_itou_nid_postfix(buffer, 0, 7, 10, 1) == 0, "itou accepted a zero capacity");
    Require(uprv_itou_nid_postfix(buffer, 16, 7, 0, 1) == 0, "itou accepted radix zero");
    Require(uprv_itou_nid_postfix(buffer, 16, 7, 37, 1) == 0, "itou accepted an out-of-range radix");
    Require(uprv_itou_nid_postfix(nullptr, 16, 7, 10, 1) == 0, "itou accepted a null buffer");
    Require(uprv_itou_nid_postfix(buffer, 2, 255, 16, 8) == 2, "itou wrote past a short buffer");
    buffer[2] = 0;
    Require(buffer[0] == 0x46 && buffer[1] == 0x46 && buffer[2] == 0, "itou clamped the digits wrongly");
    Require(uprv_pathIsAbsolute_nid_postfix("/tmp/x") == 1, "an absolute path was rejected");
    Require(uprv_pathIsAbsolute_nid_postfix("tmp/x") == 0, "a relative path was accepted");
    Require(uprv_pathIsAbsolute_nid_postfix("") == 0, "the empty path was accepted");
    Require(uprv_pathIsAbsolute_nid_postfix("C:\\x") == 0, "a drive letter was accepted on POSIX");

    Require(uprv_compareASCIIPropertyNames_nid_postfix("Script", "script") == 0, "case was not ignored");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("  sc_ript ", "SCRIPT") == 0,
        "separators were not ignored");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("script", "scx") < 0, "the comparison order is wrong");
    Require(uprv_compareASCIIPropertyNames_nid_postfix("scx", "script") > 0, "the comparison order is not symmetric");

    std::int32_t values[] = {5, 3, 9, 1, 3, 7};
    std::int32_t status = ZeroError;
    uprv_sortArray_nid_postfix(values, 6, sizeof(std::int32_t), CompareInts, nullptr, 1, &status);
    Require(status == ZeroError, "sortArray reported an error");
    const std::int32_t expected[] = {1, 3, 3, 5, 7, 9};
    for (int index = 0; index < 6; ++index) {
        Require(values[index] == expected[index], "sortArray produced the wrong order");
    }

    std::int32_t key = 7;
    std::int32_t sorted[] = {1, 5, 9};
    const auto position = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(sorted), 3, &key,
        sizeof(std::int32_t), CompareInts, nullptr);
    Require(position < 0, "the binary search found an absent item");
    Require(~position == 2, "the binary search returned the wrong insertion point");


    {
        std::int32_t sorted[] = {1, 3, 3, 3, 5, 7, 7, 9, 11, 13, 13, 13, 15, 17, 19, 19};
        const std::int32_t count = static_cast<std::int32_t>(sizeof(sorted) / sizeof(sorted[0]));
        std::int32_t key = 13;
        const auto last = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(sorted), count, &key,
            sizeof(std::int32_t), CompareInts, nullptr);
        Require(last >= 0, "the search did not find a duplicated key");
        Require(sorted[last] == 13 && (last == count - 1 || sorted[last + 1] > 13),
            "the search did not return the last occurrence of the duplicated key");

        key = 3;
        const auto firstRange = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(sorted), count, &key,
            sizeof(std::int32_t), CompareInts, nullptr);
        Require(firstRange >= 0 && sorted[firstRange] == 3, "the search missed the first duplicated key");

        key = 4;
        const auto absent = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(sorted), count, &key,
            sizeof(std::int32_t), CompareInts, nullptr);
        Require(absent < 0, "the search found an absent key");
        Require(~absent == 4, "the search returned the wrong insertion point for an absent key");

        key = 0;
        const auto below = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(sorted), count, &key,
            sizeof(std::int32_t), CompareInts, nullptr);
        Require(below < 0 && ~below == 0, "the search mishandled a key below the range");
        key = 100;
        const auto above = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(sorted), count, &key,
            sizeof(std::int32_t), CompareInts, nullptr);
        Require(above < 0 && ~above == count, "the search mishandled a key above the range");

        std::int32_t small[] = {2, 4};
        key = 4;
        const auto twoElements = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(small), 2, &key,
            sizeof(std::int32_t), CompareInts, nullptr);
        Require(twoElements == 1, "the search mishandled a two-element range");
        key = 3;
        const auto twoAbsent = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(small), 2, &key,
            sizeof(std::int32_t), CompareInts, nullptr);
        Require(twoAbsent < 0 && ~twoAbsent == 1, "the search mishandled an absent key in a two-element range");
    }
    Require(utrace_getLevel_nid_postfix() >= 0, "the trace level starts negative");
    utrace_setLevel_nid_postfix(-5);
    Require(utrace_getLevel_nid_postfix() == -1, "the trace level was not clamped down");
    utrace_setLevel_nid_postfix(100);
    Require(utrace_getLevel_nid_postfix() == 9, "the trace level was not clamped up");
    utrace_setLevel_nid_postfix(4);
    Require(utrace_getLevel_nid_postfix() == 4, "the trace level was not stored");

    const void* context = nullptr;
    UTraceEntry entry = nullptr;
    UTraceExit exit = nullptr;
    UTraceData data = nullptr;
    utrace_getFunctions_nid_postfix(&context, &entry, &exit, &data);
    Require(context == nullptr && entry == nullptr && exit == nullptr && data == nullptr,
        "the trace functions are not empty to start with");

    utrace_setFunctions_nid_postfix(reinterpret_cast<const void*>(0x1234), TraceEntry, TraceExit, TraceData);
    utrace_getFunctions_nid_postfix(&context, &entry, &exit, &data);
    Require(context == reinterpret_cast<const void*>(0x1234), "the trace context was not stored");
    Require(entry == TraceEntry && exit == TraceExit && data == TraceData, "the trace functions were not stored");

    utrace_entry_nid_postfix(17);
    Require(tracedEntries == 1 && lastFunction == 17, "utrace_entry did not forward the call");
    utrace_exit_nid_postfix(23, 1, 5);
    Require(tracedExits == 1 && lastFunction == 23, "utrace_exit did not forward the call");
    utrace_data_nid_postfix(29, 2, "value %d", 5);
    Require(tracedData == 1 && lastFunction == 29 && lastFormatMatched, "utrace_data did not forward the call");

    utrace_setFunctions_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    utrace_entry_nid_postfix(31);
    Require(tracedEntries == 1, "utrace_entry called a removed handler");
    return 0;
}
