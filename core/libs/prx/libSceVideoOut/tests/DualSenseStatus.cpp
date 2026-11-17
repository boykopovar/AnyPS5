#include "prx/libSceVideoOut/include/DualSenseStatus.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "dualsense_status: %s\n", message);
    std::exit(1);
}

void ParseUsb() {
    std::array<std::uint8_t, 64> report{};
    report[0] = 0x01;
    report[42] = 0x21;
    report[43] = 0x10;
    report[48] = 0x23;
    DualSenseStatus::TriggerStatus status;
    Require(DualSenseStatus::Parse(report.data(), report.size(), status), "a USB report is parsed");
    Require(status.leftEffect == 2 && status.rightEffect == 3, "the USB effect nibbles are split");
    Require(status.leftStatus == 1 && status.rightStatus == 2, "the USB status nibbles are split");
}

void ParseBluetooth() {
    std::array<std::uint8_t, 78> report{};
    report[0] = 0x31;
    report[43] = 0x30;
    report[44] = 0x02;
    report[49] = 0x31;
    DualSenseStatus::TriggerStatus status;
    Require(DualSenseStatus::Parse(report.data(), report.size(), status), "a Bluetooth report is parsed");
    Require(status.leftEffect == 3 && status.rightEffect == 1, "the Bluetooth effect nibbles are split");
    Require(status.leftStatus == 0 && status.rightStatus == 3, "the Bluetooth status nibbles are split");
}

void RejectOtherReports() {
    std::array<std::uint8_t, 78> report{};
    std::array<std::uint8_t, 1> empty{};
    DualSenseStatus::TriggerStatus status;
    Require(!DualSenseStatus::Parse(nullptr, 0, status), "a null report is rejected");
    Require(!DualSenseStatus::Parse(empty.data(), 0, status), "an empty report is rejected");
    report[0] = 0x01;
    Require(!DualSenseStatus::Parse(report.data(), 10, status), "a short report is rejected");
    report[0] = 0x31;
    Require(!DualSenseStatus::Parse(report.data(), 64, status), "a short Bluetooth report is rejected");
    report[0] = 0x02;
    Require(!DualSenseStatus::Parse(report.data(), report.size(), status), "an unknown report id is rejected");
}

}

int main() {
    ParseUsb();
    ParseBluetooth();
    RejectOtherReports();
    std::puts("dualsense status tests passed");
    return 0;
}
