#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal standard stream init: %s\n", message);
    std::abort();
}

std::string FileText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

using Member = void (APS5_VABI*)(void*);

void WriteStandardStreams(const char* outText, const char* errText) {
    Require(fputs_nid_postfix(outText, &_Stdout_nid_postfix) >= 0, "the guest standard output accepts the text");
    Require(fputs_nid_postfix(errText, &_Stderr_nid_postfix) >= 0, "the guest standard error accepts the text");
}

void CheckBeforeInit(Member destroy, const std::filesystem::path& outPath, const std::filesystem::path& errPath,
    const char* outText, const char* errText, const std::string& outBefore, const std::string& errBefore) {
    char object = 0;
    WriteStandardStreams(outText, errText);
    destroy(&object);
    Require(FileText(outPath) == outBefore && FileText(errPath) == errBefore, "a destruction before any construction flushes nothing");
    Require(object == 0, "the object is left untouched");
}

void CheckDualRelease(Member construct, Member firstDestroy, Member secondDestroy,
    const std::filesystem::path& outPath, const std::filesystem::path& errPath,
    const char* outText, const char* errText, std::string& outExpected, std::string& errExpected,
    const char* pendingOut = "", const char* pendingErr = "") {
    char first = 0;
    char second = 0;
    construct(&first);
    construct(&second);
    WriteStandardStreams(outText, errText);
    firstDestroy(&first);
    Require(FileText(outPath) == outExpected && FileText(errPath) == errExpected,
        "a destruction that leaves a construction counted flushes nothing");
    secondDestroy(&second);
    outExpected += pendingOut;
    outExpected += outText;
    errExpected += pendingErr;
    errExpected += errText;
    Require(FileText(outPath) == outExpected && FileText(errPath) == errExpected,
        "the last destruction flushes both guest standard output and error");
    Require(first == 0 && second == 0, "the objects are left untouched");
}

}

int main() {
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string outName = "guest_libc_internal_standard_stream_init_out_" + stamp + ".txt";
    const std::string errName = "guest_libc_internal_standard_stream_init_err_" + stamp + ".txt";
    const std::filesystem::path outPath(outName);
    const std::filesystem::path errPath(errName);
    Require(freopen_nid_postfix(outName.c_str(), "w", &_Stdout_nid_postfix) == &_Stdout_nid_postfix, "the guest standard output opens the file");
    Require(setvbuf_nid_postfix(&_Stdout_nid_postfix, nullptr, 0, 4096) == 0, "the guest standard output is fully buffered");
    Require(freopen_nid_postfix(errName.c_str(), "w", &_Stderr_nid_postfix) == &_Stderr_nid_postfix, "the guest standard error opens the file");
    Require(setvbuf_nid_postfix(&_Stderr_nid_postfix, nullptr, 0, 4096) == 0, "the guest standard error is fully buffered");

    std::string outExpected;
    std::string errExpected;

    CheckBeforeInit(libSceLibcInternalInventory_145_nid_no_patch, outPath, errPath, "narrow-pre-out|", "narrow-pre-err|", outExpected, errExpected);
    CheckDualRelease(libSceLibcInternalInventory_144_nid_no_patch, libSceLibcInternalInventory_145_nid_no_patch, libSceLibcInternalInventory_145_nid_no_patch,
        outPath, errPath, "narrow-out|", "narrow-err|", outExpected, errExpected, "narrow-pre-out|", "narrow-pre-err|");

    CheckBeforeInit(_ZNSt6_WinitD1Ev_nid_postfix, outPath, errPath, "wide-d1-pre-out|", "wide-d1-pre-err|", outExpected, errExpected);
    CheckDualRelease(libSceLibcInternalInventory_071_nid_no_patch, _ZNSt6_WinitD1Ev_nid_postfix, _ZNSt6_WinitD2Ev_nid_postfix,
        outPath, errPath, "wide-d1-d2-out|", "wide-d1-d2-err|", outExpected, errExpected, "wide-d1-pre-out|", "wide-d1-pre-err|");

    CheckBeforeInit(_ZNSt6_WinitD2Ev_nid_postfix, outPath, errPath, "wide-d2-pre-out|", "wide-d2-pre-err|", outExpected, errExpected);
    CheckDualRelease(libSceLibcInternalInventory_071_nid_no_patch, _ZNSt6_WinitD2Ev_nid_postfix, _ZNSt6_WinitD1Ev_nid_postfix,
        outPath, errPath, "wide-d2-d1-out|", "wide-d2-d1-err|", outExpected, errExpected, "wide-d2-pre-out|", "wide-d2-pre-err|");

    CheckDualRelease(libSceLibcInternalInventory_071_nid_no_patch, _ZNSt6_WinitD2Ev_nid_postfix, _ZNSt6_WinitD2Ev_nid_postfix,
        outPath, errPath, "wide-d2-d2-out|", "wide-d2-d2-err|", outExpected, errExpected);

    char single = 0;
    libSceLibcInternalInventory_071_nid_no_patch(&single);
    WriteStandardStreams("wide-single-d2-out|", "wide-single-d2-err|");
    _ZNSt6_WinitD2Ev_nid_postfix(&single);
    outExpected += "wide-single-d2-out|";
    errExpected += "wide-single-d2-err|";
    Require(FileText(outPath) == outExpected && FileText(errPath) == errExpected,
        "a single wide init release through D2 flushes both guest standard output and error");
    Require(single == 0, "the single wide object is left untouched");

    Require(fclose_nid_postfix(&_Stdout_nid_postfix) == 0, "the guest standard output closes");
    Require(fclose_nid_postfix(&_Stderr_nid_postfix) == 0, "the guest standard error closes");
    std::filesystem::remove(outPath);
    std::filesystem::remove(errPath);
    std::fputs("libc internal standard stream init checks passed\n", stderr);
    return 0;
}
