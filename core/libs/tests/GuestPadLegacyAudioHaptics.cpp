#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void RequireAt(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) RequireAt((value), __LINE__)

int main() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    std::uint32_t seen = ~std::uint32_t{0};
    PadOutputState snapshot{};
    Require(PadFetchOutput_nid_postfix(&seen, &snapshot));
    Require(snapshot.audioVibrationLarge == 0);
    Require(snapshot.audioVibrationSmall == 0);

    bool threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(0, 1000, 2000, 50, 50);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(-1, 1000, 2000, 50, 50);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(-999, 1000, 2000, 50, 50);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 2000, 2000, 50, 50);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 2500, 2000, 50, 50);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 5000, 0, 50, 50);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    PadQueueLegacyAudioHaptics_nid_postfix(1, 1000000000ULL, 2000000000ULL, 80, 40);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 1500000000ULL, 2500000000ULL, 80, 40);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        PadQueueLegacyAudioHaptics_nid_postfix(1, 500000000ULL, 1500000000ULL, 80, 40);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    PadQueueLegacyAudioHaptics_nid_postfix(1, 2000000000ULL, 3000000000ULL, 60, 30);
    PadQueueLegacyAudioHaptics_nid_postfix(1, 3500000000ULL, 4000000000ULL, 40, 20);

    PadQueueLegacyAudioHaptics_nid_postfix(2, 1500000000ULL, 2500000000ULL, 90, 10);

    PadClearLegacyAudioHaptics_nid_postfix(1);
    PadQueueLegacyAudioHaptics_nid_postfix(1, 1200000000ULL, 1800000000ULL, 70, 35);
    PadClearLegacyAudioHaptics_nid_postfix(1);
    PadClearLegacyAudioHaptics_nid_postfix(2);

    PadClearLegacyAudioHaptics_nid_postfix(999);
    PadClearLegacyAudioHaptics_nid_postfix(1);

    PadQueueLegacyAudioHaptics_nid_postfix(1, 0, ~std::uint64_t{0}, 180, 90);
    Require(PadFetchOutput_nid_postfix(&seen, &snapshot));
    Require(snapshot.audioVibrationLarge == 180);
    Require(snapshot.audioVibrationSmall == 90);

    PadQueueLegacyAudioHaptics_nid_postfix(2, 0, ~std::uint64_t{0}, 120, 210);
    Require(PadFetchOutput_nid_postfix(&seen, &snapshot));
    Require(snapshot.audioVibrationLarge == 180);
    Require(snapshot.audioVibrationSmall == 210);

    PadClearLegacyAudioHaptics_nid_postfix(1);
    Require(PadFetchOutput_nid_postfix(&seen, &snapshot));
    Require(snapshot.audioVibrationLarge == 120);
    Require(snapshot.audioVibrationSmall == 210);

    PadClearLegacyAudioHaptics_nid_postfix(2);
    Require(PadFetchOutput_nid_postfix(&seen, &snapshot));
    Require(snapshot.audioVibrationLarge == 0);
    Require(snapshot.audioVibrationSmall == 0);

    PadSubmitAudioHaptics_nid_postfix(50, 70);
    PadQueueLegacyAudioHaptics_nid_postfix(3, 0, ~std::uint64_t{0}, 100, 40);
    Require(PadFetchOutput_nid_postfix(&seen, &snapshot));
    Require(snapshot.audioVibrationLarge == 100);
    Require(snapshot.audioVibrationSmall == 70);

    PadClearLegacyAudioHaptics_nid_postfix(3);
    Require(PadFetchOutput_nid_postfix(&seen, &snapshot));
    Require(snapshot.audioVibrationLarge == 50);
    Require(snapshot.audioVibrationSmall == 70);

    PadSubmitAudioHaptics_nid_postfix(0, 0);
    PadClearLegacyAudioHaptics_nid_postfix(3);

    return 0;
}
