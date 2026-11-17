#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "Testing.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string FileText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

using Member = void (APS5_VABI*)(void*);

void CheckRelease(Member construct, Member destroy, const std::filesystem::path& path, const char* text, const std::string& before) {
    char object = 0;
    Require(fputs_nid_postfix(text, &_Stdout_nid_postfix) >= 0, "the guest standard output accepts the text");
    destroy(&object);
    Require(FileText(path) == before, "a destruction before any construction flushes nothing");
    construct(&object);
    construct(&object);
    destroy(&object);
    Require(FileText(path) == before, "a destruction that leaves a construction counted flushes nothing");
    destroy(&object);
    Require(FileText(path) == before + text, "the last destruction flushes the guest standard output");
    Require(object == 0, "the object is left untouched");
}

}

int main() {
    const std::string name = "guest_libc_internal_standard_stream_init_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt";
    const std::filesystem::path path(name);
    Require(freopen_nid_postfix(name.c_str(), "w", &_Stdout_nid_postfix) == &_Stdout_nid_postfix, "the guest standard output opens the file");
    Require(setvbuf_nid_postfix(&_Stdout_nid_postfix, nullptr, 0, 4096) == 0, "the guest standard output is fully buffered");
    CheckRelease(libSceLibcInternalInventory_144_nid_no_patch, libSceLibcInternalInventory_145_nid_no_patch, path, "narrow", "");
    CheckRelease(libSceLibcInternalInventory_071_nid_no_patch, _ZNSt6_WinitD1Ev_nid_postfix, path, "wide", "narrow");
    Require(fclose_nid_postfix(&_Stdout_nid_postfix) == 0, "the guest standard output closes");
    std::filesystem::remove(path);
    std::fputs("libc internal standard stream init checks passed\n", stderr);
    return 0;
}
