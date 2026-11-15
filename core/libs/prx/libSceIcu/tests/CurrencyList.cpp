#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocation = 7;
constexpr std::int32_t Unsupported = 16;
constexpr std::uint32_t All = 0x7fffffff;
constexpr std::uint32_t Common = 1;
constexpr std::uint32_t Uncommon = 2;
constexpr std::uint32_t Deprecated = 4;
constexpr std::uint32_t NonDeprecated = 8;

std::vector<void*> live;
std::int32_t allocationsLeft = -1;
std::int32_t unknownFrees = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU currency list: %s\n", message);
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (allocationsLeft == 0) return nullptr;
    if (allocationsLeft > 0) --allocationsLeft;
    void* pointer = std::malloc(size);
    live.push_back(pointer);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    const auto found = std::find(live.begin(), live.end(), pointer);
    if (found == live.end()) {
        ++unknownFrees;
        return;
    }
    live.erase(found);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

bool Matches(std::uint32_t type, std::uint32_t wanted) {
    return wanted == All || (type & wanted) == wanted;
}

std::vector<std::pair<std::string, std::uint32_t>> SourceTable() {
    std::ifstream source(ANYPS5_ICU_CURRENCY_SOURCE);
    Require(source.good(), "the bundled ucurr.cpp opens");
    std::vector<std::pair<std::string, std::uint32_t>> table;
    bool inside = false;
    for (std::string line; std::getline(source, line);) {
        if (!inside) {
            inside = line.find("gCurrencyList[] = {") != std::string::npos;
            continue;
        }
        const auto open = line.find("{\"");
        if (open == std::string::npos) break;
        Require(line.size() > open + 6 && line[open + 5] == '"', "a table row holds a three-letter code");
        std::uint32_t type = 0;
        if (line.find("UCURR_COMMON") != std::string::npos) type |= Common;
        if (line.find("UCURR_UNCOMMON") != std::string::npos) type |= Uncommon;
        if (line.find("UCURR_DEPRECATED") != std::string::npos) type |= Deprecated;
        if (line.find("UCURR_NON_DEPRECATED") != std::string::npos) type |= NonDeprecated;
        table.emplace_back(line.substr(open + 2, 3), type);
    }
    return table;
}

std::vector<std::string> Codes(std::uint32_t type) {
    std::int32_t status = Success;
    UEnumeration* enumeration = ucurr_openISOCurrencies(type, &status);
    Require(enumeration && status == Success, "the enumeration opens");
    const std::int32_t count = uenum_count_nid_postfix(enumeration, &status);
    std::vector<std::string> codes;
    for (;;) {
        std::int32_t length = -1;
        const char* code = uenum_next_nid_postfix(enumeration, &length, &status);
        if (!code) {
            Require(length == 0, "the end reports length 0");
            break;
        }
        Require(length == 3 && code[3] == 0, "a code is three characters and terminated");
        codes.emplace_back(code);
    }
    Require(status == Success, "enumerating leaves the status");
    Require(count == static_cast<std::int32_t>(codes.size()), "count is the number of codes next returns");
    uenum_close_nid_postfix(enumeration);
    return codes;
}

bool Has(const std::vector<std::string>& codes, const char* code) {
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

void BundledTable() {
    const auto table = SourceTable();
    Require(table.size() == 308, "ICU 78.1 lists 308 currencies");
    constexpr std::array<std::uint32_t, 15> types{All,
                                                  0,
                                                  Common,
                                                  Uncommon,
                                                  Deprecated,
                                                  NonDeprecated,
                                                  Common | NonDeprecated,
                                                  Common | Deprecated,
                                                  Uncommon | NonDeprecated,
                                                  Uncommon | Deprecated,
                                                  Common | Uncommon,
                                                  Deprecated | NonDeprecated,
                                                  16,
                                                  Common | 16,
                                                  0xffffffffu};
    for (const std::uint32_t type : types) {
        std::vector<std::string> expected;
        for (const auto& [code, flags] : table)
            if (Matches(flags, type)) expected.push_back(code);
        Require(Codes(type) == expected, "the enumeration is the bundled table filtered by type");
    }
}

void Selection() {
    const auto all = Codes(All);
    Require(all.size() == 308 && all.front() == "ADP" && all.back() == "ZWR", "every currency");
    Require(std::is_sorted(all.begin(), all.end()) && std::adjacent_find(all.begin(), all.end()) == all.end(),
            "codes are sorted and unique");
    Require(Codes(0) == all, "type 0 selects every currency");
    const auto current = Codes(Common | NonDeprecated);
    Require(current.size() == 159 && current.front() == "AED" && current.back() == "ZWG", "common current currencies");
    Require(Has(current, "USD") && Has(current, "EUR") && Has(current, "JPY") && !Has(current, "DEM") && !Has(current, "XXX"),
            "common current currencies hold USD, EUR and JPY but neither DEM nor XXX");
    const auto retired = Codes(Common | Deprecated);
    Require(retired.size() == 111 && Has(retired, "DEM") && !Has(retired, "EUR"), "common deprecated currencies");
    const auto special = Codes(Uncommon | NonDeprecated);
    Require(special.size() == 27 && Has(special, "XXX") && Has(special, "XTS") && !Has(special, "USD"), "uncommon current currencies");
    Require(Codes(Common).size() == 270 && Codes(Uncommon).size() == 38, "common and uncommon currencies");
    Require(Codes(Deprecated).size() == 122 && Codes(NonDeprecated).size() == 186, "deprecated and current currencies");
    Require(Codes(Uncommon | Deprecated).size() == 11, "uncommon deprecated currencies");
    Require(Codes(Common | Uncommon).empty() && Codes(Deprecated | NonDeprecated).empty() && Codes(16).empty(),
            "a type no currency has selects nothing");
}

bool Equal(const char16_t* units, const char* code) {
    return units && units[0] == static_cast<unsigned char>(code[0]) && units[1] == static_cast<unsigned char>(code[1]) &&
           units[2] == static_cast<unsigned char>(code[2]) && units[3] == 0;
}

void Iteration() {
    Require(live.empty(), "nothing is allocated before opening");
    std::int32_t status = Success;
    UEnumeration* enumeration = ucurr_openISOCurrencies(Common | NonDeprecated, &status);
    Require(enumeration && status == Success && live.size() == 2, "open allocates the enumeration and its context");
    Require(enumeration->baseContext == nullptr && enumeration->close && enumeration->count && enumeration->uNext && enumeration->next &&
                enumeration->reset,
            "every callback is set and no buffer is held");
    Require(uenum_count_nid_postfix(enumeration, &status) == 159, "count before iterating");
    std::int32_t length = -1;
    const char* code = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(code && std::string(code) == "AED" && length == 3, "first code");
    code = uenum_next_nid_postfix(enumeration, nullptr, &status);
    Require(code && std::string(code) == "AFN", "next without a length");
    length = -1;
    const char16_t* units = uenum_unext_nid_postfix(enumeration, &length, &status);
    Require(Equal(units, "ALL") && length == 3 && live.size() == 3 && enumeration->baseContext != nullptr,
            "unext converts the code into a buffer the enumeration owns");
    const char16_t* again = uenum_unext_nid_postfix(enumeration, nullptr, &status);
    Require(again == units && Equal(again, "AMD") && live.size() == 3, "unext reuses its buffer");
    Require(uenum_count_nid_postfix(enumeration, &status) == 159, "count does not depend on the position");
    std::int32_t remaining = 0;
    while (uenum_next_nid_postfix(enumeration, &length, &status)) ++remaining;
    Require(remaining == 155 && length == 0 && status == Success, "the rest of the list, then the end");
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr && length == 0, "the end repeats");
    length = -1;
    Require(uenum_unext_nid_postfix(enumeration, &length, &status) == nullptr && length == 0 && status == Success, "unext at the end");
    enumeration->reset(enumeration, &status);
    code = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(code && std::string(code) == "AED" && length == 3 && status == Success, "reset restarts the list");
    Require(enumeration->count(enumeration, nullptr) == 159, "the count callback ignores the status");
    code = enumeration->next(enumeration, nullptr, nullptr);
    Require(code && std::string(code) == "AFN", "the next callback ignores the status and a null length");
    enumeration->reset(enumeration, nullptr);
    Require(Equal(enumeration->uNext(enumeration, nullptr, &status), "AED"), "the reset callback ignores the status");

    std::int32_t otherStatus = Success;
    UEnumeration* other = ucurr_openISOCurrencies(All, &otherStatus);
    Require(other && other != enumeration && live.size() == 5, "a second enumeration");
    code = uenum_next_nid_postfix(other, &length, &otherStatus);
    Require(code && std::string(code) == "ADP", "enumerations keep their own type and position");
    code = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(code && std::string(code) == "AFN", "the first enumeration continues");
    uenum_close_nid_postfix(other);
    Require(live.size() == 3, "close frees an enumeration without a buffer");

    const auto next = enumeration->next;
    enumeration->next = nullptr;
    length = -1;
    Require(enumeration->uNext(enumeration, &length, &status) == nullptr && status == Unsupported && length == 0,
            "unext without a next callback is unsupported");
    enumeration->next = next;
    uenum_close_nid_postfix(enumeration);
    Require(live.empty(), "close frees the enumeration, its context and its buffer");
}

void Status() {
    std::int32_t status = IllegalArgument;
    UEnumeration* enumeration = ucurr_openISOCurrencies(All, &status);
    Require(enumeration && status == IllegalArgument, "open neither reads nor clears a failure status");
    Require(uenum_count_nid_postfix(enumeration, &status) == -1, "uenum_count refuses the failure status");
    uenum_close_nid_postfix(enumeration);
    status = -124;
    enumeration = ucurr_openISOCurrencies(All, &status);
    Require(enumeration && status == -124, "open keeps a warning status");
    uenum_close_nid_postfix(enumeration);
    enumeration = ucurr_openISOCurrencies(All, nullptr);
    Require(enumeration != nullptr, "open succeeds without a status");
    uenum_close_nid_postfix(enumeration);
    Require(live.empty(), "every enumeration is freed");
}

void AllocationFailure() {
    std::int32_t status = Success;
    allocationsLeft = 0;
    Require(ucurr_openISOCurrencies(All, &status) == nullptr && status == MemoryAllocation && live.empty(),
            "a failed enumeration allocation");
    status = Success;
    allocationsLeft = 1;
    Require(ucurr_openISOCurrencies(All, &status) == nullptr && status == MemoryAllocation && live.empty(),
            "a failed context allocation frees the enumeration");
    allocationsLeft = 0;
    Require(Rejects([] { ucurr_openISOCurrencies(All, nullptr); }) && live.empty(), "a failed allocation without a status");
    allocationsLeft = 1;
    Require(Rejects([] { ucurr_openISOCurrencies(All, nullptr); }) && live.empty(), "a failed context allocation without a status");

    allocationsLeft = -1;
    status = Success;
    UEnumeration* enumeration = ucurr_openISOCurrencies(All, &status);
    Require(enumeration && status == Success, "open after failures");
    allocationsLeft = 0;
    std::int32_t length = -1;
    Require(uenum_unext_nid_postfix(enumeration, &length, &status) == nullptr && status == MemoryAllocation && length == 3 &&
                enumeration->baseContext == nullptr,
            "a failed buffer allocation reports the code length and an error");
    allocationsLeft = -1;
    status = Success;
    Require(Equal(uenum_unext_nid_postfix(enumeration, &length, &status), "AED") && status == Success,
            "the code whose buffer failed is consumed");
    uenum_close_nid_postfix(enumeration);
    Require(live.empty(), "close after a failed buffer allocation");
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    BundledTable();
    Selection();
    Iteration();
    Status();
    AllocationFailure();
    Require(unknownFrees == 0, "only pointers the allocator returned are freed, once each");
    return 0;
}
