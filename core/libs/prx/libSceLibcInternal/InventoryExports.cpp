#include <cstdint>
#include <limits>
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/CodecvtUtf.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
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

int APS5_VABI _ZNKSt22_System_error_category23default_error_conditionEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNKSt22_System_error_category7messageEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb0EEC2EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb0EEC2ERKSt8_Locinfomb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb0EEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb0EED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb0EED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb0EED2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EE7_GetcatEPPKNSt6locale5facetEPKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EEC1EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EEC1ERKSt8_Locinfomb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EEC2EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt10moneypunctIwLb1EED2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt12future_errorD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt12system_errorC2ESt10error_codePKc(LibcException::SystemErrorObject* self, LibcException::ErrorCode code, const char* message) {
    LibcExceptionConstructSystemError_nid_no_patch(self, code, message);
}

int APS5_VABI _ZNSt13bad_exceptionD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt13bad_exceptionD1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt13bad_exceptionD2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE4syncEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE5uflowEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE6setbufEPci() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE7seekoffElNSt5_IosbIiE8_SeekdirENS4_9_OpenmodeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE7seekposESt4fposI9_MbstatetENSt5_IosbIiE9_OpenmodeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE8overflowEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEED2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE6setbufEPwi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE7seekposESt4fposI9_MbstatetENSt5_IosbIiE9_OpenmodeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE8overflowEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE9pbackfailEi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_istreamIwSt11char_traitsIwEED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_ostreamIwSt11char_traitsIwEE6sentryC2ERS2_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_ostreamIwSt11char_traitsIwEE6sentryD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_ostreamIwSt11char_traitsIwEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt13basic_ostreamIwSt11char_traitsIwEED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

std::streamsize APS5_VABI _ZNSt15basic_streambufIcSt11char_traitsIcEE9showmanycEv(std::basic_streambuf<char>*) {
    return 0;
}

std::streamsize APS5_VABI _ZNSt15basic_streambufIwSt11char_traitsIwEE9showmanycEv(std::basic_streambuf<wchar_t>*) {
    return 0;
}


int APS5_VABI _ZNSt24_Iostream_error_categoryD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}



int APS5_VABI _ZNSt5ctypeIwED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6_Mutex5_LockEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6_Mutex7_UnlockEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6_MutexC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6_MutexD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt6_WinitC1Ev(void*) {
    CountStandardStreamInit(wideStreamInitCount);
}

void APS5_VABI _ZNSt6_WinitD1Ev_nid_postfix(void*) {
    ReleaseStandardStreamInit(wideStreamInitCount);
}

void APS5_VABI _ZNSt6_WinitD2Ev_nid_postfix(void*) {
    ReleaseStandardStreamInit(wideStreamInitCount);
}

int APS5_VABI _ZNSt6_WinitC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6locale16_SetgloballocaleEPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6locale5emptyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6locale7_Locimp8_MakelocERKSt8_LocinfoiPS0_PKS_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6locale7_Locimp9_MakewlocERKSt8_LocinfoiPS0_PKS_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6locale7_Locimp9_MakexlocERKSt8_LocinfoiPS0_PKS_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6locale7_LocimpD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt6locale7_LocimpD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceLibcInternalInventory_083_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pMWnITHysPc", libSceLibcInternalInventory_083_nid_no_patch);

int APS5_VABI _ZNSt7_MpunctIcE5_InitERKSt8_Locinfob() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7_MpunctIcEC2EPKcmbb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7_MpunctIcED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7_MpunctIwE5_InitERKSt8_Locinfob() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7_MpunctIwEC2EPKcmbb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7_MpunctIwED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt7codecvtIDic9_MbstatetED0Ev(void* self) {
    CodecvtUtf32Delete_nid_no_patch(self);
}

void APS5_VABI _ZNSt7codecvtIDic9_MbstatetED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7codecvtIDsc9_MbstatetED0Ev(void* self) {
    CodecvtUtf16Delete_nid_no_patch(self);
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7codecvtIwc9_MbstatetED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7collateIcED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7collateIcED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt7collateIwE5_InitERKSt8_Locinfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7collateIwEC1EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7collateIwEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7collateIwEC2EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7collateIwEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7collateIwEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7collateIwED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt7collateIwED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7collateIwED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}

int APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}

int APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt8_Locinfo8_AddcatsEiPKc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8_LocinfoC1ERKSs() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8_LocinfoC2ERKSs() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8_LocinfoC2EiPKc() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8ios_base4InitC1Ev(void*) {
    CountStandardStreamInit(narrowStreamInitCount);
}

void APS5_VABI _ZNSt8ios_base4InitD1Ev(void*) {
    ReleaseStandardStreamInit(narrowStreamInitCount);
}

int APS5_VABI _ZNSt8ios_base4InitD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8ios_base5clearENSt5_IosbIiE8_IostateEb() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8messagesIcE5_InitERKSt8_Locinfo(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}

int APS5_VABI _ZNSt8messagesIcE7_GetcatEPPKNSt6locale5facetEPKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8messagesIcEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8messagesIcEC2EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8messagesIcED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt8messagesIcED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt8messagesIwE7_GetcatEPPKNSt6locale5facetEPKS1_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8messagesIwEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8messagesIwEC2EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8messagesIwEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8messagesIwED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt8messagesIwED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt8numpunctIcE5_InitERKSt8_Locinfob() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8numpunctIcE5_TidyEv(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt8numpunctIcEC1ERKSt8_Locinfomb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIcEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIcEC2EPKcmb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIcEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIcED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8numpunctIwE5_TidyEv(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt8numpunctIwEC1EPKcmb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwEC1ERKSt8_Locinfomb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwEC2EPKcmb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwEC2ERKSt8_Locinfomb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8numpunctIwED2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_TidyEv(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2EPKcm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_TidyEv(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9basic_iosIcSt11char_traitsIcEE4initEPSt15basic_streambufIcS1_Eb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9basic_iosIwSt11char_traitsIwEE4initEPSt15basic_streambufIwS1_Eb() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}

int APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}

int APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_St22_String_const_iteratorISt11_String_valISt13_Simple_typesIcEEEm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo(void* self, const void* locinfo) {
    (void)self;
    (void)locinfo;
}

int APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZNSt9type_infoD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt9type_infoD1Ev(void* self) {
    (void)self;
}

void APS5_VABI _ZNSt9type_infoD2Ev(void* self) {
    (void)self;
}

int APS5_VABI _ZSt10_GetloctxtIcSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZSt10_GetloctxtIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT_() {
    NotImplemented_nid_no_patch(__func__);
}

[[noreturn]] void APS5_VABI _ZSt10_Rng_abortPKc(const char* message) {
    if (message == nullptr) NotImplemented_nid_no_patch("std::_Rng_abort with a null message");
    _Thrd_abort_nid_postfix(message);
}

int APS5_VABI _ZSt14_Debug_messagePKcS0_j() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZSt22_Random_device_entropyv() {
    NotImplemented_nid_no_patch(__func__);
}

[[noreturn]] void APS5_VABI _ZSt6_ThrowRKSt9exception(const LibcException::ExceptionObject* error) {
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

int APS5_VABI _ZSt9_LStrxfrmIcEmPT_S1_PKS0_S3_PKSt8_Collvec() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZSt9_LStrxfrmIwEmPT_S1_PKS0_S3_PKSt8_Collvec() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZTv0_n24_NSiD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZTv0_n24_NSoD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZTv0_n24_NSoD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZTv0_n24_NSt13basic_istreamIwSt11char_traitsIwEED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZTv0_n24_NSt13basic_ostreamIwSt11char_traitsIwEED0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZTv0_n24_NSt13basic_ostreamIwSt11char_traitsIwEED1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

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


void APS5_VABI _init_tls(void) {
}

std::uint64_t APS5_VABI _malloc_finalize_lv2(void) {
    return 0;
}

int APS5_VABI _malloc_init_lv2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _malloc_init() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _malloc_postfork() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _malloc_prefork() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _new_setup() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _rtld_atfork_pre(int* locks) {
    (void)locks;
}

void APS5_VABI _rtld_error(const char* format, ...) {
    (void)format;
}

int APS5_VABI _rtld_get_stack_prot(void) {
    return 7;
}

int APS5_VABI _rtld_thread_init() {
    NotImplemented_nid_no_patch(__func__);
}

std::uint64_t APS5_VABI _sceLibcGetMallocParam(void) {
    return 0;
}

int APS5_VABI backtrace_game() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI clock_1700() {
    NotImplemented_nid_no_patch(__func__);
}

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
