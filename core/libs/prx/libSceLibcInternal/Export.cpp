#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" void APS5_VABI sceKernelSetThreadDtors(thread_dtors_func_t dtors);
extern "C" int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t address, int flags, ModuleInfoEx* info);
extern "C" const char* __progname_nid_postfix;

extern "C" {

int APS5_VABI sceLibcInternalPending4_001_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8bad_cast8_DoraiseEv", sceLibcInternalPending4_001_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_002_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8ios_base7failure8_DoraiseEv", sceLibcInternalPending4_002_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_003_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIcE3getEiiiRKSs", sceLibcInternalPending4_003_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_004_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIcE4openERKSsRKSt6locale", sceLibcInternalPending4_004_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_005_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIcE5closeEi", sceLibcInternalPending4_005_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_006_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIcE6do_getEiiiRKSs", sceLibcInternalPending4_006_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_007_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIcE7do_openERKSsRKSt6locale", sceLibcInternalPending4_007_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_008_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIcE8do_closeEi", sceLibcInternalPending4_008_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_009_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE", sceLibcInternalPending4_009_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_010_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIwE4openERKSsRKSt6locale", sceLibcInternalPending4_010_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_011_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIwE5closeEi", sceLibcInternalPending4_011_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_012_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE", sceLibcInternalPending4_012_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_013_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIwE7do_openERKSsRKSt6locale", sceLibcInternalPending4_013_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_014_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8messagesIwE8do_closeEi", sceLibcInternalPending4_014_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_015_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE11do_groupingEv", sceLibcInternalPending4_015_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_016_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE11do_truenameEv", sceLibcInternalPending4_016_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_017_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE12do_falsenameEv", sceLibcInternalPending4_017_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_018_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE13decimal_pointEv", sceLibcInternalPending4_018_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_019_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE13thousands_sepEv", sceLibcInternalPending4_019_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_020_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE16do_decimal_pointEv", sceLibcInternalPending4_020_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_021_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE16do_thousands_sepEv", sceLibcInternalPending4_021_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_022_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE8groupingEv", sceLibcInternalPending4_022_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_023_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE8truenameEv", sceLibcInternalPending4_023_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_024_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIcE9falsenameEv", sceLibcInternalPending4_024_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_025_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE11do_groupingEv", sceLibcInternalPending4_025_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_026_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE11do_truenameEv", sceLibcInternalPending4_026_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_027_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE12do_falsenameEv", sceLibcInternalPending4_027_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_028_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE13decimal_pointEv", sceLibcInternalPending4_028_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_029_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE13thousands_sepEv", sceLibcInternalPending4_029_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_030_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE16do_decimal_pointEv", sceLibcInternalPending4_030_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_031_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE16do_thousands_sepEv", sceLibcInternalPending4_031_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_032_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE8groupingEv", sceLibcInternalPending4_032_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_033_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE8truenameEv", sceLibcInternalPending4_033_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_034_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8numpunctIwE9falsenameEv", sceLibcInternalPending4_034_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_035_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE10date_orderEv", sceLibcInternalPending4_035_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_036_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_036_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_037_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_037_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_038_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_038_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_039_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_039_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_040_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE13do_date_orderEv", sceLibcInternalPending4_040_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_041_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE13get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_041_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_042_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_042_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_043_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_043_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_044_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc", sceLibcInternalPending4_044_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_045_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc", sceLibcInternalPending4_045_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_046_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc", sceLibcInternalPending4_046_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_047_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetintERS3_S5_iiRiRKSt5ctypeIcE", sceLibcInternalPending4_047_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_048_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_048_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_049_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_049_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_050_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_050_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_051_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE10date_orderEv", sceLibcInternalPending4_051_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_052_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_052_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_053_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_053_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_054_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_054_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_055_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_055_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_056_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE13do_date_orderEv", sceLibcInternalPending4_056_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_057_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE13get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_057_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_058_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_058_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_059_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_059_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_060_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE_", sceLibcInternalPending4_060_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_061_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc", sceLibcInternalPending4_061_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_062_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc", sceLibcInternalPending4_062_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_063_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc", sceLibcInternalPending4_063_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_064_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE", sceLibcInternalPending4_064_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_065_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_065_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_066_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_066_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_067_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm", sceLibcInternalPending4_067_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_068_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecPK2tmPKcSB_", sceLibcInternalPending4_068_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_069_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_RSt8ios_basecPK2tmcc", sceLibcInternalPending4_069_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_070_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecPK2tmcc", sceLibcInternalPending4_070_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_071_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmPKwSB_", sceLibcInternalPending4_071_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_072_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPK2tmcc", sceLibcInternalPending4_072_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_073_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPK2tmcc", sceLibcInternalPending4_073_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_074_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9bad_alloc8_DoraiseEv", sceLibcInternalPending4_074_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_075_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSs", sceLibcInternalPending4_075_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_076_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending4_076_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_077_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSs", sceLibcInternalPending4_077_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_078_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending4_078_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_079_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetmfldERS3_S5_bRSt8ios_basePc", sceLibcInternalPending4_079_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_080_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSbIwS2_SaIwEE", sceLibcInternalPending4_080_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_081_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending4_081_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_082_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSbIwS2_SaIwEE", sceLibcInternalPending4_082_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_083_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe", sceLibcInternalPending4_083_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_084_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetmfldERS3_S5_bRSt8ios_basePw", sceLibcInternalPending4_084_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_085_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_bRSt8ios_basecRKSs", sceLibcInternalPending4_085_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_086_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_bRSt8ios_basece", sceLibcInternalPending4_086_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_087_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_bRSt8ios_basecRKSs", sceLibcInternalPending4_087_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_088_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_bRSt8ios_basece", sceLibcInternalPending4_088_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_089_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE8_PutmfldES3_bRSt8ios_basecbSsc", sceLibcInternalPending4_089_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_090_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_bRSt8ios_basewRKSbIwS2_SaIwEE", sceLibcInternalPending4_090_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_091_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_bRSt8ios_basewe", sceLibcInternalPending4_091_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_092_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewRKSbIwS2_SaIwEE", sceLibcInternalPending4_092_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_093_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewe", sceLibcInternalPending4_093_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_094_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE8_PutmfldES3_bRSt8ios_basewbSbIwS2_SaIwEEw", sceLibcInternalPending4_094_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_095_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm", sceLibcInternalPending4_095_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_096_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm", sceLibcInternalPending4_096_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_097_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm", sceLibcInternalPending4_097_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_098_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE6appendEmw", sceLibcInternalPending4_098_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_099_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm", sceLibcInternalPending4_099_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_100_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm", sceLibcInternalPending4_100_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_101_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE6assignEmw", sceLibcInternalPending4_101_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_102_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw", sceLibcInternalPending4_102_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_103_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSiD0Ev", sceLibcInternalPending4_103_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_104_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSiD1Ev", sceLibcInternalPending4_104_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_105_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSo6sentryC2ERSo", sceLibcInternalPending4_105_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_106_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSo6sentryD2Ev", sceLibcInternalPending4_106_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_107_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSoD0Ev", sceLibcInternalPending4_107_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_108_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSoD1Ev", sceLibcInternalPending4_108_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_109_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs5_CopyEmm", sceLibcInternalPending4_109_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_110_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs5eraseEmm", sceLibcInternalPending4_110_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_111_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs6appendERKSsmm", sceLibcInternalPending4_111_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_112_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs6appendEmc", sceLibcInternalPending4_112_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_113_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs6assignEPKcm", sceLibcInternalPending4_113_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_114_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs6assignERKSsmm", sceLibcInternalPending4_114_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_115_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs6assignEmc", sceLibcInternalPending4_115_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_116_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSs6insertEmmc", sceLibcInternalPending4_116_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_117_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem10_Copy_fileEPKcS1_", sceLibcInternalPending4_117_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_118_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem10_File_sizeEPKc", sceLibcInternalPending4_118_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_119_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem11_EquivalentEPKcS1_", sceLibcInternalPending4_119_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_120_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem12_Current_setEPKc", sceLibcInternalPending4_120_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_121_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem13_Current_set2EPKc", sceLibcInternalPending4_121_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_122_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem16_Last_write_timeEPKc", sceLibcInternalPending4_122_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_123_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem18_Xfilesystem_errorEPKcRKNS_4pathES4_St10error_code", sceLibcInternalPending4_123_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_124_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem20_Set_last_write_timeEPKcl", sceLibcInternalPending4_124_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_125_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem5_StatEPKcPNS_5permsE", sceLibcInternalPending4_125_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_126_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem6_LstatEPKcPNS_5permsE", sceLibcInternalPending4_126_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_127_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem7_RenameEPKcS1_", sceLibcInternalPending4_127_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_128_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem7_ResizeEPKcm", sceLibcInternalPending4_128_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_129_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10filesystem8_StatvfsEPKcRNS_10space_infoE", sceLibcInternalPending4_129_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_130_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EE7_GetcatEPPKNSt6locale5facetEPKS1_", sceLibcInternalPending4_130_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_131_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EEC1EPKcm", sceLibcInternalPending4_131_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_132_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EEC1ERKSt8_Locinfomb", sceLibcInternalPending4_132_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_133_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EEC1Em", sceLibcInternalPending4_133_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_134_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EEC2EPKcm", sceLibcInternalPending4_134_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_135_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EEC2ERKSt8_Locinfomb", sceLibcInternalPending4_135_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_136_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EEC2Em", sceLibcInternalPending4_136_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_137_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EED0Ev", sceLibcInternalPending4_137_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_138_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EED1Ev", sceLibcInternalPending4_138_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_139_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb0EED2Ev", sceLibcInternalPending4_139_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_140_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EE7_GetcatEPPKNSt6locale5facetEPKS1_", sceLibcInternalPending4_140_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_141_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EEC1EPKcm", sceLibcInternalPending4_141_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_142_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EEC1ERKSt8_Locinfomb", sceLibcInternalPending4_142_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_143_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EEC1Em", sceLibcInternalPending4_143_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_144_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EEC2EPKcm", sceLibcInternalPending4_144_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_145_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EEC2ERKSt8_Locinfomb", sceLibcInternalPending4_145_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_146_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EEC2Em", sceLibcInternalPending4_146_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_147_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EED0Ev", sceLibcInternalPending4_147_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_148_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EED1Ev", sceLibcInternalPending4_148_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_149_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIcLb1EED2Ev", sceLibcInternalPending4_149_nid_no_patch);

int APS5_VABI sceLibcInternalPending4_150_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("_ZNSt10moneypunctIwLb0EE7_GetcatEPPKNSt6locale5facetEPKS1_", sceLibcInternalPending4_150_nid_no_patch);

}

namespace {

using ThreadDestructorFunction = void (APS5_VABI*)(void*);

struct ThreadDestructor {
    ThreadDestructorFunction function;
    void* object;
    void* dsoSymbol;
};

struct ThreadDestructorsTag {};

std::vector<ThreadDestructor>& ThreadDestructors() {
    return HostThreadLocal<std::vector<ThreadDestructor>, ThreadDestructorsTag>();
}

bool IsInLoadedImage(const void* address) {
    if (address == nullptr)
        return false;
#ifdef _WIN32
    HMODULE module = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(address), &module) != 0;
#else
    Dl_info info{};
    return dladdr(address, &info) != 0;
#endif
}

void CallThreadDestructor(const ThreadDestructor& destructor) {
    const auto* function = reinterpret_cast<const void*>(destructor.function);
    if (!IsInLoadedImage(function)) {
        std::ostringstream message;
        message << "thread_local destructor " << function << " of dso " << destructor.dsoSymbol << " is not in a loaded image";
        throw std::runtime_error(message.str());
    }
    destructor.function(destructor.object);
}

void APS5_VABI RunThreadDestructors_nid_no_patch() {
    auto& destructors = ThreadDestructors();
    while (!destructors.empty()) {
        const ThreadDestructor destructor = destructors.back();
        destructors.pop_back();
        CallThreadDestructor(destructor);
    }
}

bool FindDsoModule(const void* dsoSymbol, KernelModule& handle) {
    ModuleInfoEx info{};
    info.st_size = sizeof(ModuleInfoEx);
    if (sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(dsoSymbol), 2, &info) != 0) {
        handle = 0;
        return false;
    }
    handle = info.id;
    return true;
}

bool ForceThreadDestructorPass(KernelModule handle) {
    auto& destructors = ThreadDestructors();
    bool found = false;
    for (std::size_t index = destructors.size(); index-- > 0;) {
        const ThreadDestructor destructor = destructors[index];
        KernelModule module = 0;
        const bool loaded = FindDsoModule(destructor.dsoSymbol, module);
        if (module != handle)
            continue;
        found = true;
        destructors.erase(destructors.begin() + static_cast<std::ptrdiff_t>(index));
        if (loaded && *static_cast<void* const*>(destructor.dsoSymbol) == destructor.dsoSymbol)
            CallThreadDestructor(destructor);
    }
    return found;
}

void RegisterThreadExitHook() {
    [[maybe_unused]] static const bool registered = [] {
        sceKernelSetThreadDtors(RunThreadDestructors_nid_no_patch);
        return true;
    }();
}

}

extern "C" {

int Need_sceLibcInternal_nid_postfix = 1;

const char* APS5_VABI getprogname_nid_postfix() {
    return __progname_nid_postfix;
}

const char* APS5_VABI _getprogname_nid_postfix() {
    return getprogname_nid_postfix();
}

void APS5_VABI __cxa_finalize_nid_postfix(void* dsoHandle) {
    CxaFinalize_nid_no_patch(dsoHandle);
}

void APS5_VABI sceLibcHeapGetTraceInfo_nid_postfix(Info* info) {
    LibcHeapTraceInfo_nid_no_patch(info);
}

int APS5_VABI _sceLibcInternalThreadAtexit_nid_postfix(ThreadDestructorFunction destructor, void* object, void* dsoSymbol) {
    RegisterThreadExitHook();
    ThreadDestructors().push_back({destructor, object, dsoSymbol});
    return 0;
}

void APS5_VABI _sceLibcInternalThreadDtors_nid_postfix() {
    RunThreadDestructors_nid_no_patch();
}

int APS5_VABI _sceLibcInternalForceTlsDestructor_nid_postfix(KernelModule handle) {
    for (int pass = 0; pass < 4; ++pass) {
        if (!ForceThreadDestructorPass(handle))
            break;
    }
    return 0;
}

unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int seconds);

unsigned int APS5_VABI alarm_nid_postfix(unsigned int seconds) {
    return GuestAlarm_nid_no_patch(seconds);
}
}
