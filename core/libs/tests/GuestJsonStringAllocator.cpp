#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct StringTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    int budget = -1;
    int notifications = 0;
};
static void* APS5_VABI AllocateString(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<StringTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) return nullptr;
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); return memory;
}
static void APS5_VABI FreeString(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<StringTrace*>(context);
    Require(trace.live.erase(memory) == 1, "String must free through its owning allocation context"); std::free(memory);
}
static void APS5_VABI NotifyString(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<StringTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "String failure notification must identify the failed request"); ++trace.notifications;
}
static void* const StringVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateString), reinterpret_cast<void*>(FreeString), reinterpret_cast<void*>(NotifyString)};
struct StringAllocator { void* const* vtable = StringVtable; };
static void CheckText(const String& text, const char* bytes, std::size_t length, std::size_t capacity) {
    Require(_ZNK3sce4Json6String4sizeEv(&text) == length && _ZNK3sce4Json6String8capacityEv(&text) == capacity &&
            std::memcmp(_ZNK3sce4Json6String5c_strEv(&text), bytes, length + 1) == 0, "String bytes, length and capacity must match the reference");
}
int main() {
    Value beforeInitialization{};
    _ZN3sce4Json5ValueC1Ev(&beforeInitialization);
    _ZNK3sce4Json5Value9getStringEv(&beforeInitialization);
    _ZN3sce4Json5ValueD1Ev(&beforeInitialization);
    StringTrace trace;
    StringAllocator allocator;
    Parameter parameter{&allocator, &trace, 16384, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize string allocation contract");
    const auto baseline = trace.live.size();
    for (int budget : {0, 1}) {
        trace.budget = budget;
        String failed{}; _ZN3sce4Json6StringC1EPKc(&failed, "abc");
        Require(budget ? trace.live.count(failed.text) == 1 : failed.text == nullptr, "A header failure and a character failure retain distinct partial ownership");
        CheckText(failed, "", 0, 0);
        trace.budget = -1;
        _ZN3sce4Json6StringD1Ev(&failed);
        Require(!failed.text && trace.live.size() == baseline, "Failed string construction must be destructible without leaking");
    }
    String text{}; _ZN3sce4Json6StringC1EPKc(&text, "abc");
    Require(trace.live.at(text.text) == 24 && trace.live.at(*reinterpret_cast<void**>(text.text)) == 4, "Actual string storage must use native allocator-backed header and characters");
    for (unsigned i = 16; i < 24; ++i) Require(reinterpret_cast<const unsigned char*>(text.text)[i] == 0xa5, "String construction must preserve reserved header bytes");
    CheckText(text, "abc", 3, 4);
    trace.budget = 0;
    _ZN3sce4Json6String7reserveEm(&text, 17); CheckText(text, "abc", 3, 4);
    _ZN3sce4Json6String6resizeEm(&text, 18); CheckText(text, "abc", 3, 4);
    trace.budget = -1;
    _ZN3sce4Json6String7reserveEm(&text, 17); CheckText(text, "abc", 3, 17);
    _ZN3sce4Json6String6resizeEm(&text, 18);
    char grown[19]{}; std::memcpy(grown, "abc", 3); CheckText(text, grown, 18, 25);
    _ZN3sce4Json6String6resizeEm(&text, 2); CheckText(text, "ab", 2, 25);
    _ZN3sce4Json6String5clearEv(&text); CheckText(text, "", 0, 0);
    const char binary[]{'a', 0, 'b', 0};
    _ZN3sce4Json6String6appendEPKcm(&text, binary, 3); CheckText(text, binary, 3, 20);
    String copy{}; _ZN3sce4Json6StringC1ERKS1_(&copy, &text); CheckText(copy, binary, 3, 4);
    String appended{}; _ZN3sce4Json6StringC1EPKc(&appended, nullptr);
    _ZN3sce4Json6String6appendERKS1_(&appended, &text); CheckText(appended, "a", 1, 18);
    _ZN3sce4Json6StringpLEh(&appended, 0); CheckText(appended, "a", 1, 18);
    _ZN3sce4Json6StringaSERKS1_(&appended, &text); CheckText(appended, binary, 3, 4);
    trace.budget = 0;
    _ZN3sce4Json6StringaSERKS1_(&appended, &copy); CheckText(appended, "", 0, 0);
    trace.budget = -1;
    Require(trace.notifications == 5, "Each header, character, growth and assignment failure must notify once");
    _ZN3sce4Json6StringD1Ev(&appended); _ZN3sce4Json6StringD1Ev(&copy); _ZN3sce4Json6StringD1Ev(&text);
    Require(trace.live.size() == baseline, "String operations must release all owned resources");
    Value null{}; _ZN3sce4Json5ValueC1Ev(&null);
    const auto* defaultString = _ZNK3sce4Json5Value9getStringEv(&null);
    Require(defaultString && defaultString->text && trace.live.at(defaultString->text) == 24, "The default string header must belong to the active allocator");
    _ZN3sce4Json5ValueD1Ev(&null);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Terminate string allocation contract");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Reinitialize default string ownership");
    _ZN3sce4Json5ValueC1Ev(&null);
    defaultString = _ZNK3sce4Json5Value9getStringEv(&null);
    Require(defaultString && defaultString->text && trace.live.at(defaultString->text) == 24, "A new initialization must own its own default string header");
    _ZN3sce4Json5ValueD1Ev(&null);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Release defaults from repeated initialization");
    return 0;
}
