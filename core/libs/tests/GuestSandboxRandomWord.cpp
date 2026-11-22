#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>

static void Require(bool value) { if (!value) std::abort(); }

#ifdef __linux__
extern "C" void ReadThermalSensors_nid_no_patch(const char* root, int celsius[3]);

static void WriteText(const std::filesystem::path& file, const std::string& content) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary);
    stream << content;
}

static void TestThermalReaderPlausibilityAndFiltering() {
    const auto base = std::filesystem::temp_directory_path() / "anyps5-thermal-reader-regression";
    std::error_code cleanupError;
    std::filesystem::remove_all(base, cleanupError);

    const auto highBogusRoot = base / "high-bogus";
    WriteText(highBogusRoot / "hwmon" / "hwmon0" / "name", "coretemp\n");
    WriteText(highBogusRoot / "hwmon" / "hwmon0" / "temp1_input", "1500000\n");
    WriteText(highBogusRoot / "hwmon" / "hwmon1" / "name", "k10temp\n");
    WriteText(highBogusRoot / "hwmon" / "hwmon1" / "temp1_input", "58000\n");
    int highBogus[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch(highBogusRoot.string().c_str(), highBogus);
    Require(highBogus[0] == 58 && highBogus[1] == -1 && highBogus[2] == -1);

    const auto negativeBogusRoot = base / "negative-bogus";
    WriteText(negativeBogusRoot / "hwmon" / "hwmon0" / "name", "coretemp\n");
    WriteText(negativeBogusRoot / "hwmon" / "hwmon0" / "temp1_input", "-50000\n");
    WriteText(negativeBogusRoot / "hwmon" / "hwmon1" / "name", "k10temp\n");
    WriteText(negativeBogusRoot / "hwmon" / "hwmon1" / "temp1_input", "58000\n");
    int negativeBogus[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch(negativeBogusRoot.string().c_str(), negativeBogus);
    Require(negativeBogus[0] == 58 && negativeBogus[1] == -1 && negativeBogus[2] == -1);

    const auto thermalFilterRoot = base / "thermal-filter";
    WriteText(thermalFilterRoot / "thermal" / "thermal_zone0" / "type", "iwlwifi_1\n");
    WriteText(thermalFilterRoot / "thermal" / "thermal_zone0" / "temp", "41000\n");
    WriteText(thermalFilterRoot / "thermal" / "thermal_zone1" / "type", "acpitz\n");
    WriteText(thermalFilterRoot / "thermal" / "thermal_zone1" / "temp", "37000\n");
    int thermalFiltered[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch(thermalFilterRoot.string().c_str(), thermalFiltered);
    Require(thermalFiltered[0] == -1 && thermalFiltered[1] == 37 && thermalFiltered[2] == -1);

    const auto crlfGpuRoot = base / "crlf-gpu";
    WriteText(crlfGpuRoot / "hwmon" / "hwmon0" / "name", "xe\r\n");
    WriteText(crlfGpuRoot / "hwmon" / "hwmon0" / "temp1_input", "62000\n");
    int crlfGpu[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch(crlfGpuRoot.string().c_str(), crlfGpu);
    Require(crlfGpu[0] == 62 && crlfGpu[1] == -1 && crlfGpu[2] == -1);

    ReadThermalSensors_nid_no_patch(crlfGpuRoot.string().c_str(), nullptr);
    ReadThermalSensors_nid_no_patch(nullptr, nullptr);

    std::filesystem::remove_all(base, cleanupError);
}
#endif

int main() {
    const char* word = sceKernelGetFsSandboxRandomWord();
    Require(word != nullptr);
    const std::size_t length = std::strlen(word);
    Require(length >= 1);
    for (std::size_t index = 0; index < length; ++index)
        Require(std::isalnum(static_cast<unsigned char>(word[index])));

    Require(sceKernelGetFsSandboxRandomWord() == word);
    const char* fromThread = nullptr;
    std::thread([&] { fromThread = sceKernelGetFsSandboxRandomWord(); }).join();
    Require(fromThread == word);

    const std::string path = std::string("/") + word + "/common/lib/libc.sprx";
    *__error_nid_postfix() = 0;
    Require(open_nid_postfix(path.c_str(), 0, 0) == -1);
    Require(*__error_nid_postfix() == 2);

#ifdef __linux__
    TestThermalReaderPlausibilityAndFiltering();
#endif
    return 0;
}
