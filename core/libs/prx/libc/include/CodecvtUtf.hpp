#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

inline constexpr std::size_t kCodecvtUtfVtableSlots = 13;
static_assert(kCodecvtUtfVtableSlots * sizeof(std::uintptr_t) == 104);

extern "C" {

extern std::uint64_t _ZNSt7codecvtIDsc9_MbstatetE2idE_nid_postfix;
extern std::uint64_t _ZNSt7codecvtIDic9_MbstatetE2idE_nid_postfix;
extern std::uintptr_t _ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix[kCodecvtUtfVtableSlots];
extern std::uintptr_t _ZTVSt7codecvtIDic9_MbstatetE_nid_postfix[kCodecvtUtfVtableSlots];

void APS5_VABI CodecvtUtf16Destroy_nid_no_patch(void* self);
void APS5_VABI CodecvtUtf16Delete_nid_no_patch(void* self);
bool APS5_VABI CodecvtUtf16AlwaysNoconv_nid_no_patch(const void* self);
int APS5_VABI CodecvtUtf16MaxLength_nid_no_patch(const void* self);
int APS5_VABI CodecvtUtf16Encoding_nid_no_patch(const void* self);
int APS5_VABI CodecvtUtf16In_nid_no_patch(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char16_t* first2, char16_t* last2, char16_t** mid2);
int APS5_VABI CodecvtUtf16Out_nid_no_patch(const void* self, void* state, const char16_t* first1, const char16_t* last1, const char16_t** mid1, char* first2, char* last2, char** mid2);
int APS5_VABI CodecvtUtf16Unshift_nid_no_patch(const void* self, void* state, char* first, char* last, char** next);
int APS5_VABI CodecvtUtf16Length_nid_no_patch(const void* self, void* state, const char* first, const char* last, std::size_t count);

void APS5_VABI CodecvtUtf32Destroy_nid_no_patch(void* self);
void APS5_VABI CodecvtUtf32Delete_nid_no_patch(void* self);
bool APS5_VABI CodecvtUtf32AlwaysNoconv_nid_no_patch(const void* self);
int APS5_VABI CodecvtUtf32MaxLength_nid_no_patch(const void* self);
int APS5_VABI CodecvtUtf32Encoding_nid_no_patch(const void* self);
int APS5_VABI CodecvtUtf32In_nid_no_patch(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char32_t* first2, char32_t* last2, char32_t** mid2);
int APS5_VABI CodecvtUtf32Out_nid_no_patch(const void* self, void* state, const char32_t* first1, const char32_t* last1, const char32_t** mid1, char* first2, char* last2, char** mid2);
int APS5_VABI CodecvtUtf32Unshift_nid_no_patch(const void* self, void* state, char* first, char* last, char** next);
int APS5_VABI CodecvtUtf32Length_nid_no_patch(const void* self, void* state, const char* first, const char* last, std::size_t count);

void APS5_VABI _ZNSt7codecvtIDsc9_MbstatetED0Ev_nid_postfix(void* self);
bool APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE16do_always_noconvEv_nid_postfix(const void* self);
int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE11do_encodingEv_nid_postfix(const void* self);
int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6__nid_postfix(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char16_t* first2, char16_t* last2, char16_t** mid2);
int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6__nid_postfix(const void* self, void* state, const char16_t* first1, const char16_t* last1, const char16_t** mid1, char* first2, char* last2, char** mid2);

void APS5_VABI _ZNSt7codecvtIDic9_MbstatetED1Ev_nid_postfix(void* self);
void APS5_VABI _ZNSt7codecvtIDic9_MbstatetED0Ev_nid_postfix(void* self);
bool APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE16do_always_noconvEv_nid_postfix(const void* self);
int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6__nid_postfix(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char32_t* first2, char32_t* last2, char32_t** mid2);
int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6__nid_postfix(const void* self, void* state, const char32_t* first1, const char32_t* last1, const char32_t** mid1, char* first2, char* last2, char** mid2);
int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(const void* self, void* state, const char* first, const char* last, std::size_t count);

}
