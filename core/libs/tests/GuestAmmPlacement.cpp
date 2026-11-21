#include "prx/libSceAmpr/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static const char* gCapturedConstraintMessage = nullptr;
static void* gCapturedConstraintPointer = nullptr;
static int gCapturedConstraintError = 0;
static int gCapturedConstraintCalls = 0;

static void APS5_VABI TestConstraintHandler(const char* message, void* pointer, int error) {
    gCapturedConstraintMessage = message;
    gCapturedConstraintPointer = pointer;
    gCapturedConstraintError = error;
    ++gCapturedConstraintCalls;
}

static void VerifyDefaultSonyLconv(const SonyLconv* locale) {
    Require(locale != nullptr, "SonyLocaleconv returned null");
    Require(locale->decimal_point != nullptr && std::strcmp(locale->decimal_point, ".") == 0, "decimal_point was not default '.'");
    Require(locale->grouping != nullptr && std::strcmp(locale->grouping, "") == 0, "grouping was not empty");
    Require(locale->thousands_sep != nullptr && std::strcmp(locale->thousands_sep, "") == 0, "thousands_sep was not empty");
    Require(locale->frac_grouping != nullptr && std::strcmp(locale->frac_grouping, "") == 0, "frac_grouping was not empty");
    Require(locale->frac_sep != nullptr && std::strcmp(locale->frac_sep, "") == 0, "frac_sep was not empty");
    Require(locale->false_text != nullptr && std::strcmp(locale->false_text, "false") == 0, "false_text was not 'false'");
    Require(locale->true_text != nullptr && std::strcmp(locale->true_text, "true") == 0, "true_text was not 'true'");
    Require(locale->no_text != nullptr && std::strcmp(locale->no_text, "") == 0, "no_text was not empty");
    Require(locale->yes_text != nullptr && std::strcmp(locale->yes_text, "") == 0, "yes_text was not empty");
    for (const auto byte : locale->chars) {
        Require(byte == CHAR_MAX, "SonyLconv chars element was not CHAR_MAX");
    }
    for (const auto* pointer : locale->monetary) {
        Require(pointer != nullptr && std::strcmp(pointer, "") == 0, "SonyLconv monetary element was not empty");
    }
}

int main() {
    try {
        std::uintptr_t arenaBase = 0;
        std::size_t arenaBytes = 0;
        GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
        Require(arenaBase != 0 && arenaBytes != 0, "the guest arena is unavailable");
        const std::uint64_t arenaEnd = arenaBase + arenaBytes;
        constexpr std::uint64_t occupied = 4ull << 20;
        GuestArena::GuestArenaMarkUsed_nid_postfix(reinterpret_cast<void*>(arenaEnd - occupied), occupied);

        std::uint64_t start = 0;
        std::uint64_t end = 0;
        std::uint64_t multimapStart = 0;
        std::uint64_t multimapEnd = 0;
        Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0, "AMM ranges were not reported");
        Require(end - start == 256ull << 30 && end == multimapStart && multimapEnd - multimapStart == 32ull << 30, "AMM ranges are not a 256 GiB range and an adjacent 32 GiB multimap range");
        Require(start >= arenaBase && multimapEnd <= arenaEnd - occupied, "AMM ranges overlap the used top of the guest arena");
        Require(start % (64ull << 30) == 0, "the AMM range does not start on a 64 GiB boundary");
        Require(GuestArena::GuestArenaHighestFree_nid_no_patch(multimapEnd - start, 64ull << 30) < start, "AMM ranges are not at the top of the free guest arena");

        std::uint64_t start2 = 0;
        std::uint64_t end2 = 0;
        std::uint64_t multimapStart2 = 0;
        std::uint64_t multimapEnd2 = 0;
        Require(sceAmprAmmGetVirtualAddressRanges(&start2, &end2, &multimapStart2, &multimapEnd2) == 0, "second AMM range query failed");
        Require(start2 == start && end2 == end && multimapStart2 == multimapStart && multimapEnd2 == multimapEnd, "AMM ranges changed on repeated query");

        void* const initialHandler = SonySetConstraintHandlerS_nid_no_patch(nullptr);
        Require(initialHandler != nullptr, "initial constraint handler was null");
        SonyInvokeConstraintHandler_nid_no_patch(nullptr, nullptr, 0);

        gCapturedConstraintMessage = nullptr;
        gCapturedConstraintPointer = nullptr;
        gCapturedConstraintError = 0;
        gCapturedConstraintCalls = 0;
        void* const previousDefault = SonySetConstraintHandlerS_nid_no_patch(reinterpret_cast<void*>(&TestConstraintHandler));
        Require(previousDefault == initialHandler, "SonySetConstraintHandlerS did not return the previous default handler");

        const char* const testMessage = "constraint check";
        int dummyTarget = 42;
        SonyInvokeConstraintHandler_nid_no_patch(testMessage, &dummyTarget, 22);
        Require(gCapturedConstraintCalls == 1, "custom constraint handler was not invoked");
        Require(gCapturedConstraintMessage == testMessage, "custom constraint handler did not receive message");
        Require(gCapturedConstraintPointer == &dummyTarget, "custom constraint handler did not receive pointer");
        Require(gCapturedConstraintError == 22, "custom constraint handler did not receive error");

        void* const previousCustom = SonySetConstraintHandlerS_nid_no_patch(nullptr);
        Require(previousCustom == reinterpret_cast<void*>(&TestConstraintHandler), "SonySetConstraintHandlerS did not return the previous custom handler");

        SonyLconv* const initialLocale = SonyLocaleconv_nid_no_patch();
        VerifyDefaultSonyLconv(initialLocale);

        char customPoint[] = ",";
        char customGrouping[] = "\x03";
        char customMonetary[] = "$";
        initialLocale->decimal_point[0] = ',';
        initialLocale->decimal_point = customPoint;
        initialLocale->grouping = customGrouping;
        initialLocale->chars[0] = 3;
        initialLocale->monetary[0] = customMonetary;

        SonyLconv* const resetLocale = SonyLocaleconv_nid_no_patch();
        VerifyDefaultSonyLconv(resetLocale);

        std::puts("AMM ranges placed below the used top of the guest arena");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
