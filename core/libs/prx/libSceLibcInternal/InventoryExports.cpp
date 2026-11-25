#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/CodecvtUtf.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include <mutex>

namespace {

std::mutex standardStreamInitMutex;
int narrowStreamInitCount = -1;
int wideStreamInitCount = -1;

void CountStandardStreamInit(int& count) {
    std::lock_guard lock(standardStreamInitMutex);
    count = count < 0 ? 1 : count + 1;
}

void ReleaseStandardStreamInit(int& count) {
    int remaining = 0;
    {
        std::lock_guard lock(standardStreamInitMutex);
        remaining = --count;
    }
    if (remaining != 0) return;
    for (FileStream* stream : {&_Stdout_nid_postfix, &_Stderr_nid_postfix}) {
        if (stream->IsOpen()) fflush_nid_postfix(stream);
    }
}

}

extern "C" {

[[noreturn]] void abort_nid_postfix(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

int APS5_VABI libSceLibcInternalInventory_000_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ww3UUl317Ng", libSceLibcInternalInventory_000_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_002_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HpAlvhNKb2Y", libSceLibcInternalInventory_002_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_003_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5bQqdR3hTZw", libSceLibcInternalInventory_003_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_004_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1kvQkOSaaVo", libSceLibcInternalInventory_004_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_005_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("bBGvmspg3Xs", libSceLibcInternalInventory_005_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_006_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("95MaQlRbfC8", libSceLibcInternalInventory_006_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_007_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ki5SQPsB4m4", libSceLibcInternalInventory_007_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_008_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6F1JfiING18", libSceLibcInternalInventory_008_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_009_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HmcMLz3cPkM", libSceLibcInternalInventory_009_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_010_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pyBabUesXN4", libSceLibcInternalInventory_010_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_011_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uROsAczW6OU", libSceLibcInternalInventory_011_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_012_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("X69UlAXF-Oc", libSceLibcInternalInventory_012_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_013_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GS1AvxBwVgY", libSceLibcInternalInventory_013_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_014_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sTaUDDnGOpE", libSceLibcInternalInventory_014_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_015_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("OM0FnA7Tldk", libSceLibcInternalInventory_015_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_016_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uVOxy7dQTFc", libSceLibcInternalInventory_016_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_017_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rNYLEsL7M0k", libSceLibcInternalInventory_017_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_019_nid_no_patch(LibcException::SystemErrorObject* self, LibcException::ErrorCode code, const char* message) {
    LibcExceptionConstructSystemError_nid_no_patch(self, code, message);
}
APS5_EXPORT("oly3wSwEJ2A", libSceLibcInternalInventory_019_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_021_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("t6egllDqQ2M", libSceLibcInternalInventory_021_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_022_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("iWNC2tkDgxw", libSceLibcInternalInventory_022_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_023_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VNaqectsZNs", libSceLibcInternalInventory_023_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_025_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jfr41GGp2jA", libSceLibcInternalInventory_025_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_026_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SYFNsz9K2rs", libSceLibcInternalInventory_026_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_027_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7oio2Gs1GNk", libSceLibcInternalInventory_027_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_028_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rsS5cBMihAM", libSceLibcInternalInventory_028_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_029_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("oYMRgkQHoJM", libSceLibcInternalInventory_029_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_030_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MYCRRmc7cDA", libSceLibcInternalInventory_030_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_031_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yc2gZRtDeNQ", libSceLibcInternalInventory_031_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_032_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gOxGOQmSVU0", libSceLibcInternalInventory_032_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_034_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wAcnCK2HCeI", libSceLibcInternalInventory_034_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_035_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("10VcrHqHAlw", libSceLibcInternalInventory_035_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_036_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PjH5dZGfQHQ", libSceLibcInternalInventory_036_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_037_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NeiFvKblpZM", libSceLibcInternalInventory_037_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_038_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JJ-mkOhdook", libSceLibcInternalInventory_038_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_040_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4GbIwW5u5us", libSceLibcInternalInventory_040_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_041_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MB1VCygerRU", libSceLibcInternalInventory_041_nid_no_patch);

std::streamsize APS5_VABI libSceLibcInternalInventory_047_nid_no_patch(std::basic_streambuf<char>*) {
    return 0;
}
APS5_EXPORT("IAEl1Ta7yVU", libSceLibcInternalInventory_047_nid_no_patch);

std::streamsize APS5_VABI libSceLibcInternalInventory_050_nid_no_patch(std::basic_streambuf<wchar_t>*) {
    return 0;
}
APS5_EXPORT("olsoiZsezkk", libSceLibcInternalInventory_050_nid_no_patch);


int APS5_VABI libSceLibcInternalInventory_058_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("27Z-Cx1jbkU", libSceLibcInternalInventory_058_nid_no_patch);



int APS5_VABI libSceLibcInternalInventory_066_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zOPA5qnbW2U", libSceLibcInternalInventory_066_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_071_nid_no_patch(void*) {
    CountStandardStreamInit(wideStreamInitCount);
}

void APS5_VABI _ZNSt6_WinitD1Ev_nid_postfix(void*) {
    ReleaseStandardStreamInit(wideStreamInitCount);
}

void APS5_VABI _ZNSt6_WinitD2Ev_nid_postfix(void*) {
    ReleaseStandardStreamInit(wideStreamInitCount);
}
APS5_EXPORT("-Bl9-SZ2noc", libSceLibcInternalInventory_071_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_072_nid_no_patch(void* self) {
    libSceLibcInternalInventory_071_nid_no_patch(self);
}
APS5_EXPORT("57mMrw0l-40", libSceLibcInternalInventory_072_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_073_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1TaQLyPDJEY", libSceLibcInternalInventory_073_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_074_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("K-5mtupQZ4g", libSceLibcInternalInventory_074_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_077_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5r801ZWiJJI", libSceLibcInternalInventory_077_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_078_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BcbHFSrcg3Y", libSceLibcInternalInventory_078_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_079_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fkFGlPdquqI", libSceLibcInternalInventory_079_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_081_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aD-iqbVlHmQ", libSceLibcInternalInventory_081_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_082_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("So6gSmJMYDs", libSceLibcInternalInventory_082_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_083_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pMWnITHysPc", libSceLibcInternalInventory_083_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_084_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("m7qAgircaZY", libSceLibcInternalInventory_084_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_085_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0il9qdo6fhs", libSceLibcInternalInventory_085_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_086_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0AeC+qCELEA", libSceLibcInternalInventory_086_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_087_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iCoD0EOIbTM", libSceLibcInternalInventory_087_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_088_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TDhjx3nyaoU", libSceLibcInternalInventory_088_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_089_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0TADyPWrobI", libSceLibcInternalInventory_089_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_090_nid_no_patch(void* self) {
    CodecvtUtf32Delete_nid_no_patch(self);
}
APS5_EXPORT("vwMx2NhWdLw", libSceLibcInternalInventory_090_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_091_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("TuhGCIxgLvA", libSceLibcInternalInventory_091_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_092_nid_no_patch(void* self) {
    CodecvtUtf16Delete_nid_no_patch(self);
}
APS5_EXPORT("zYHryd8vd0w", libSceLibcInternalInventory_092_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_097_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("djNkrJKTb6Q", libSceLibcInternalInventory_097_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_098_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("0fkFA3za2N8", libSceLibcInternalInventory_098_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_102_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("8pXCeme0FC4", libSceLibcInternalInventory_102_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_103_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("dP5zwQ2Yc8g", libSceLibcInternalInventory_103_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_104_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LxKs-IGDsFU", libSceLibcInternalInventory_104_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_105_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fjHAU8OSaW8", libSceLibcInternalInventory_105_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_106_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wggIIjWSt-E", libSceLibcInternalInventory_106_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_107_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PSAw7g1DD24", libSceLibcInternalInventory_107_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_108_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2PoQu-K2qXk", libSceLibcInternalInventory_108_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_109_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HQbgeUdQyyw", libSceLibcInternalInventory_109_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_110_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("ig4VDIRc21Q", libSceLibcInternalInventory_110_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_111_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("ZO3a6HfALTQ", libSceLibcInternalInventory_111_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_112_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("84wIPnwBGiU", libSceLibcInternalInventory_112_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_113_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uXj-oWD2334", libSceLibcInternalInventory_113_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_114_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yAobGI2Whrg", libSceLibcInternalInventory_114_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_115_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RNmYVYlZvv8", libSceLibcInternalInventory_115_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_116_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("-1G1iE3KyGI", libSceLibcInternalInventory_116_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_117_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("kAay0hfgDJs", libSceLibcInternalInventory_117_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_118_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("6S8jzWWGcWo", libSceLibcInternalInventory_118_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_119_nid_no_patch(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}
APS5_EXPORT("4MHgRGOKOXY", libSceLibcInternalInventory_119_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_120_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0UPU3kvxWb0", libSceLibcInternalInventory_120_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_121_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("18rLbEV-NQs", libSceLibcInternalInventory_121_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_122_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("e3shgCIZxRc", libSceLibcInternalInventory_122_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_123_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-+RKa3As0gE", libSceLibcInternalInventory_123_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_124_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("aHDdLa7jA1U", libSceLibcInternalInventory_124_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_125_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("Zbaaq-d70ms", libSceLibcInternalInventory_125_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_126_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("bwVJf3kat9c", libSceLibcInternalInventory_126_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_127_nid_no_patch(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}
APS5_EXPORT("BbJ4naWZeRw", libSceLibcInternalInventory_127_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_128_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Hg1im-rUeHc", libSceLibcInternalInventory_128_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_129_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LAEVU8cBSh4", libSceLibcInternalInventory_129_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_130_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Mniutm2JL2M", libSceLibcInternalInventory_130_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_131_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("aOK5ucXO-5g", libSceLibcInternalInventory_131_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_132_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("WoCt9o2SYHw", libSceLibcInternalInventory_132_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_133_nid_no_patch(void* self, const void* info, std::size_t refs) {
    _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(
        static_cast<GuestLocale::Facet*>(self), static_cast<const GuestLocale::LocinfoStorage*>(info), refs);
}
APS5_EXPORT("Ky+C-qbKcX0", libSceLibcInternalInventory_133_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_134_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xdHqQoggdfo", libSceLibcInternalInventory_134_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_135_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("f1ZGLUnQGgo", libSceLibcInternalInventory_135_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_136_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("jyXTVnmlJD4", libSceLibcInternalInventory_136_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_137_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("WiUy3dEtCOQ", libSceLibcInternalInventory_137_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_138_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("6hV3y21d59k", libSceLibcInternalInventory_138_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_139_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WkAsdy5CUAo", libSceLibcInternalInventory_139_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_140_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2aSk2ruCP0E", libSceLibcInternalInventory_140_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_142_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ReK9U6EUWuU", libSceLibcInternalInventory_142_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_143_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("i180MNC9p4c", libSceLibcInternalInventory_143_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_144_nid_no_patch(void*) {
    CountStandardStreamInit(narrowStreamInitCount);
}
APS5_EXPORT("sqWytnhYdEg", libSceLibcInternalInventory_144_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_145_nid_no_patch(void*) {
    ReleaseStandardStreamInit(narrowStreamInitCount);
}
APS5_EXPORT("kxXCvcat1cM", libSceLibcInternalInventory_145_nid_no_patch);

void APS5_VABI _ZNSt8ios_base4InitD2Ev_nid_postfix(void*) {
    ReleaseStandardStreamInit(narrowStreamInitCount);
}

int APS5_VABI libSceLibcInternalInventory_147_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QXJCcrXoqpU", libSceLibcInternalInventory_147_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_149_nid_no_patch(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}
APS5_EXPORT("eLB2+1+mVvg", libSceLibcInternalInventory_149_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_153_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("34mi8lteNTs", libSceLibcInternalInventory_153_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_154_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("yDdbQr1oLOc", libSceLibcInternalInventory_154_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_159_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("6sAaleB7Zgk", libSceLibcInternalInventory_159_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_160_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("I-e7Dxo087A", libSceLibcInternalInventory_160_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_161_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1LvbNeZZJ-o", libSceLibcInternalInventory_161_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_162_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("fFnht9SPed8", libSceLibcInternalInventory_162_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_163_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CDm+TUClE7E", libSceLibcInternalInventory_163_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_164_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TEtyeXjcZ0w", libSceLibcInternalInventory_164_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_165_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yIn4l8OO1zA", libSceLibcInternalInventory_165_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_166_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1eVdDzPtzD4", libSceLibcInternalInventory_166_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_167_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qEob3o53s2M", libSceLibcInternalInventory_167_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_168_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("2YCDWkuFEy8", libSceLibcInternalInventory_168_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_169_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("H+T2VJ91dds", libSceLibcInternalInventory_169_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_170_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("s1EM2NdPf0Y", libSceLibcInternalInventory_170_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_171_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XOgjMgZ3fjo", libSceLibcInternalInventory_171_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_172_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("m4kEqv7eGVY", libSceLibcInternalInventory_172_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_173_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MQJQCxbLfM0", libSceLibcInternalInventory_173_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_174_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ElKI+ReiehU", libSceLibcInternalInventory_174_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_175_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VHBnRBxBg5E", libSceLibcInternalInventory_175_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_176_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lzK3uL1rWJY", libSceLibcInternalInventory_176_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_177_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XDm4jTtoEbo", libSceLibcInternalInventory_177_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_178_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("+yrOX7MgVlk", libSceLibcInternalInventory_178_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_179_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iO5AOflrTaA", libSceLibcInternalInventory_179_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_180_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("M0n7l76UVyE", libSceLibcInternalInventory_180_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_181_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("l7OtvplI42U", libSceLibcInternalInventory_181_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_182_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dU8Q2yzFNQg", libSceLibcInternalInventory_182_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_183_nid_no_patch(void* self) {
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev_nid_postfix(static_cast<GuestLocale::Facet*>(self));
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("mmq9OwwYx74", libSceLibcInternalInventory_183_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_184_nid_no_patch(void* self) {
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev_nid_postfix(static_cast<GuestLocale::Facet*>(self));
}
APS5_EXPORT("Cp9ksNOeun8", libSceLibcInternalInventory_184_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_185_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("ImblNB7fVVU", libSceLibcInternalInventory_185_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_192_nid_no_patch(void* self) {
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(static_cast<GuestLocale::Facet*>(self));
}
APS5_EXPORT("QTgRx1NTp6o", libSceLibcInternalInventory_192_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_197_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("oi3kpQPqpMY", libSceLibcInternalInventory_197_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_198_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("b1LciG4lUUk", libSceLibcInternalInventory_198_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_200_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UQPicLg8Sx8", libSceLibcInternalInventory_200_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_201_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uqLGWOX7-YE", libSceLibcInternalInventory_201_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_202_nid_no_patch(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}
APS5_EXPORT("6zg3ziZ4Qis", libSceLibcInternalInventory_202_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_203_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MSSvHmcbs3g", libSceLibcInternalInventory_203_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_204_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7NQGsY7VY3c", libSceLibcInternalInventory_204_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_205_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YGPopdkhujM", libSceLibcInternalInventory_205_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_206_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iWtXRduTjHA", libSceLibcInternalInventory_206_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_207_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("f+1EaDVL5C4", libSceLibcInternalInventory_207_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_208_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("b9QSruV4nnc", libSceLibcInternalInventory_208_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_209_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("CClObiVHzDY", libSceLibcInternalInventory_209_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_210_nid_no_patch(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}
APS5_EXPORT("JOj6qfc4VLs", libSceLibcInternalInventory_210_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_211_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ej+44l1PjjY", libSceLibcInternalInventory_211_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_212_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("RB3ratfpZDc", libSceLibcInternalInventory_212_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_213_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("a6yvHMSqsV0", libSceLibcInternalInventory_213_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_214_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("7ZmeGHyM6ew", libSceLibcInternalInventory_214_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_215_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("66AuqgLnsQE", libSceLibcInternalInventory_215_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_216_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1dY2KJfkgMM", libSceLibcInternalInventory_216_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_217_nid_no_patch(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}
APS5_EXPORT("riBxNiKLvI0", libSceLibcInternalInventory_217_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_218_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("w9fCz0pbHdw", libSceLibcInternalInventory_218_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_219_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mdYczJb+bb0", libSceLibcInternalInventory_219_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_220_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Qi5fpNt5+T0", libSceLibcInternalInventory_220_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_221_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("b2na0Dzd5j8", libSceLibcInternalInventory_221_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_222_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XqbpfYmAZB4", libSceLibcInternalInventory_222_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_223_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("s2zG12AYKTg", libSceLibcInternalInventory_223_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_224_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("AnE9WWbyWkM", libSceLibcInternalInventory_224_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_225_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("MuACiCSA8-s", libSceLibcInternalInventory_225_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_226_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6QU40olMkOM", libSceLibcInternalInventory_226_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_227_nid_no_patch(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}
APS5_EXPORT("kJmdxo4uM+8", libSceLibcInternalInventory_227_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_228_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0sHarDG9BY4", libSceLibcInternalInventory_228_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_229_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rme+Po9yI5M", libSceLibcInternalInventory_229_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_230_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jIvWFH24Bjw", libSceLibcInternalInventory_230_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_231_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("4+oswXtp7PQ", libSceLibcInternalInventory_231_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_232_nid_no_patch(void* self) {
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("N5nZ3wQw+Vc", libSceLibcInternalInventory_232_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_233_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("LLssoYjMOow", libSceLibcInternalInventory_233_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_234_nid_no_patch(void* self) {
    (void)self;
}
APS5_EXPORT("zb3436295XU", libSceLibcInternalInventory_234_nid_no_patch);

[[noreturn]] void APS5_VABI libSceLibcInternalInventory_237_nid_no_patch(const char* message) {
    if (message == nullptr) NotImplemented_nid_no_patch("std::_Rng_abort with a null message");
    _Thrd_abort_nid_postfix(message);
}
APS5_EXPORT("VrWUXy1gqn8", libSceLibcInternalInventory_237_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_239_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Zn44KZgJtWY", libSceLibcInternalInventory_239_nid_no_patch);

double APS5_VABI libSceLibcInternalInventory_244_nid_no_patch() {
    return 0.0;
}
APS5_EXPORT("mDIHE-aaRaI", libSceLibcInternalInventory_244_nid_no_patch);

[[noreturn]] void APS5_VABI libSceLibcInternalInventory_245_nid_no_patch(const LibcException::ExceptionObject* error) {
    using What = const char* (APS5_VABI *)(const void*);
    std::uintptr_t table = 0;
    What what = nullptr;
    if (!GuestCopyIn_nid_no_patch(&table, error, sizeof(table)) || table > std::numeric_limits<std::uintptr_t>::max() - 2 * sizeof(what) ||
        !GuestCopyIn_nid_no_patch(&what, reinterpret_cast<const void*>(table + 2 * sizeof(what)), sizeof(what)) || what == nullptr)
        NotImplemented_nid_no_patch("std::_Throw with an unreadable exception or what callback");
    const char* message = what(error);
    fflush_nid_postfix(nullptr);
    fprintf_nid_postfix(&_Stderr_nid_postfix, "%s(%u) : error : %s\n", message, 0u, "exception");
    abort_nid_postfix(0, 0, 0, 0, 0, 0);
}
APS5_EXPORT("pDFe-IgbTPg", libSceLibcInternalInventory_245_nid_no_patch);

std::size_t APS5_VABI libSceLibcInternalInventory_252_nid_no_patch(char* first1, char* last1, const char* first2, const char* last2, const void* collvec) {
    (void)collvec;
    if ((first1 == nullptr) != (last1 == nullptr) || last1 < first1 || (first2 == nullptr) != (last2 == nullptr) || last2 < first2)
        throw std::invalid_argument("_LStrxfrm<char>: invalid character range");
    const auto length = static_cast<std::size_t>(last2 - first2);
    if (length == 0 || length > static_cast<std::size_t>(last1 - first1)) return length;
    std::vector<char> copy(length);
    if (!GuestCopyIn_nid_no_patch(copy.data(), first2, length)) throw std::invalid_argument("_LStrxfrm<char>: unreadable source");
    if (!GuestCopyOut_nid_no_patch(first1, copy.data(), length)) throw std::invalid_argument("_LStrxfrm<char>: unwritable buffer");
    return length;
}
APS5_EXPORT("V62E2Q8bJVY", libSceLibcInternalInventory_252_nid_no_patch);

std::size_t APS5_VABI libSceLibcInternalInventory_253_nid_no_patch(char16_t* first1, char16_t* last1, const char16_t* first2, const char16_t* last2, const void* collvec) {
    (void)collvec;
    if ((first1 == nullptr) != (last1 == nullptr) || last1 < first1 || (first2 == nullptr) != (last2 == nullptr) || last2 < first2)
        throw std::invalid_argument("_LStrxfrm<wchar_t>: invalid character range");
    const auto length = static_cast<std::size_t>(last2 - first2);
    if (length == 0 || length > static_cast<std::size_t>(last1 - first1)) return length;
    std::vector<char16_t> copy(length);
    if (!GuestCopyIn_nid_no_patch(copy.data(), first2, length * sizeof(char16_t))) throw std::invalid_argument("_LStrxfrm<wchar_t>: unreadable source");
    if (!GuestCopyOut_nid_no_patch(first1, copy.data(), length * sizeof(char16_t))) throw std::invalid_argument("_LStrxfrm<wchar_t>: unwritable buffer");
    return length;
}
APS5_EXPORT("BloPUt1HCH0", libSceLibcInternalInventory_253_nid_no_patch);

int APS5_VABI __atomic_compare_exchange_n_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __atomic_exchange_n_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __atomic_store_n_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceLibcInternalInventory_263_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-m7FIvSBbMM", libSceLibcInternalInventory_263_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_264_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7ruwcMCJVGI", libSceLibcInternalInventory_264_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_265_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2Lvc7KWtErs", libSceLibcInternalInventory_265_nid_no_patch);


void APS5_VABI libSceLibcInternalInventory_268_nid_no_patch(void) {
}
APS5_EXPORT("6NCOqr3cD74", libSceLibcInternalInventory_268_nid_no_patch);

std::uint64_t APS5_VABI libSceLibcInternalInventory_269_nid_no_patch(void) {
    return 0;
}
APS5_EXPORT("RnqlvEmvkdw", libSceLibcInternalInventory_269_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_271_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("20cUk0qX9zo", libSceLibcInternalInventory_271_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_272_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("z8GPiQwaAEY", libSceLibcInternalInventory_272_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_273_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("V94pLruduLg", libSceLibcInternalInventory_273_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_274_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aLYyS4Kx6rQ", libSceLibcInternalInventory_274_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_275_nid_no_patch(const void* table) {
    return CrtNewSetup_nid_no_patch(table);
}
APS5_EXPORT("KNNNbyRieqQ", libSceLibcInternalInventory_275_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_277_nid_no_patch(int* locks) {
    (void)locks;
}
APS5_EXPORT("i-7v8+UGglc", libSceLibcInternalInventory_277_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_278_nid_no_patch(const char* format, ...) {
    (void)format;
}
APS5_EXPORT("dmHEVUqDYGw", libSceLibcInternalInventory_278_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_279_nid_no_patch(void) {
    return 7;
}
APS5_EXPORT("AdYYKgtPlqg", libSceLibcInternalInventory_279_nid_no_patch);

void APS5_VABI libSceLibcInternalInventory_280_nid_no_patch(void* lockInfo) {
    (void)lockInfo;
    libSceLibcInternalInventory_278_nid_no_patch("Service unavailable");
}
APS5_EXPORT("gRw4XrztJ4Q", libSceLibcInternalInventory_280_nid_no_patch);

std::uint64_t APS5_VABI libSceLibcInternalInventory_281_nid_no_patch(void) {
    return 0;
}
APS5_EXPORT("1nZ4Xfnyp38", libSceLibcInternalInventory_281_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_283_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jFSmlgyyEUg", libSceLibcInternalInventory_283_nid_no_patch);

int APS5_VABI libSceLibcInternalInventory_284_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("n8onIBR4Qdk", libSceLibcInternalInventory_284_nid_no_patch);

int APS5_VABI libSceLibcInternalUnknown_GG6441JdYkA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GG6441JdYkA", libSceLibcInternalUnknown_GG6441JdYkA_nid_no_patch);

int APS5_VABI libSceLibcInternalUnknown_QZ9YgTk_plus_yrE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QZ9YgTk+yrE", libSceLibcInternalUnknown_QZ9YgTk_plus_yrE_nid_no_patch);

int APS5_VABI libSceLibcInternalUnknown_bGuDd3kWVKQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("bGuDd3kWVKQ", libSceLibcInternalUnknown_bGuDd3kWVKQ_nid_no_patch);

int APS5_VABI libSceLibcInternalUnknown_f9LVyN8Ky8g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("f9LVyN8Ky8g", libSceLibcInternalUnknown_f9LVyN8Ky8g_nid_no_patch);

int APS5_VABI libSceLibcInternalUnknown_o1Ez0fsXvhI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("o1Ez0fsXvhI", libSceLibcInternalUnknown_o1Ez0fsXvhI_nid_no_patch);

int APS5_VABI libSceLibcInternalUnknown_wUqJ0psUjDo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wUqJ0psUjDo", libSceLibcInternalUnknown_wUqJ0psUjDo_nid_no_patch);

}
