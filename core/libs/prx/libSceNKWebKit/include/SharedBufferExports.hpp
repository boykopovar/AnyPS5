#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/SharedBufferAbi.hpp"

#include <cstddef>

extern "C" {

NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEv();
NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEPKcm(const char* data, std::size_t size);
NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEPKhm(const unsigned char* data, std::size_t size);
NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WtfAbi::Vector<char>* vector);
NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIhLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WtfAbi::Vector<unsigned char>* vector);
void APS5_VABI _ZN7WebCore12SharedBufferC1Ev(NKWebKit::WebCoreAbi::SharedBuffer* self);
void APS5_VABI _ZN7WebCore12SharedBufferC2Ev(NKWebKit::WebCoreAbi::SharedBuffer* self);
void APS5_VABI _ZN7WebCore12SharedBufferC1EPKcm(NKWebKit::WebCoreAbi::SharedBuffer* self, const char* data, std::size_t size);
void APS5_VABI _ZN7WebCore12SharedBufferC2EPKcm(NKWebKit::WebCoreAbi::SharedBuffer* self, const char* data, std::size_t size);
void APS5_VABI _ZN7WebCore12SharedBufferC1EPKhm(NKWebKit::WebCoreAbi::SharedBuffer* self, const unsigned char* data, std::size_t size);
void APS5_VABI _ZN7WebCore12SharedBufferC2EPKhm(NKWebKit::WebCoreAbi::SharedBuffer* self, const unsigned char* data, std::size_t size);
void APS5_VABI _ZN7WebCore12SharedBufferC1EON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::SharedBuffer* self, NKWebKit::WtfAbi::Vector<char>* vector);
void APS5_VABI _ZN7WebCore12SharedBufferC2EON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::SharedBuffer* self, NKWebKit::WtfAbi::Vector<char>* vector);
void APS5_VABI _ZN7WebCore12SharedBufferD1Ev(NKWebKit::WebCoreAbi::SharedBuffer* self);
void APS5_VABI _ZN7WebCore12SharedBufferD2Ev(NKWebKit::WebCoreAbi::SharedBuffer* self);
void APS5_VABI _ZN7WebCore12SharedBuffer6appendERKS0_(NKWebKit::WebCoreAbi::SharedBuffer* self, const NKWebKit::WebCoreAbi::SharedBuffer* data);
void APS5_VABI _ZN7WebCore12SharedBuffer6appendEPKcm(NKWebKit::WebCoreAbi::SharedBuffer* self, const char* data, std::size_t length);
void APS5_VABI _ZN7WebCore12SharedBuffer6appendEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::SharedBuffer* self, NKWebKit::WtfAbi::Vector<char>* data);
void APS5_VABI _ZN7WebCore12SharedBuffer5clearEv(NKWebKit::WebCoreAbi::SharedBuffer* self);
void APS5_VABI _ZNK7WebCore12SharedBuffer21combineIntoOneSegmentEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZNK7WebCore12SharedBuffer4copyEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
const char* APS5_VABI _ZNK7WebCore12SharedBuffer4dataEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
const unsigned char* APS5_VABI _ZNK7WebCore12SharedBuffer14dataAsUInt8PtrEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
std::size_t APS5_VABI _ZNK7WebCore12SharedBuffer4sizeEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
bool APS5_VABI _ZNK7WebCore12SharedBuffer7isEmptyEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
const NKWebKit::WebCoreAbi::DataSegmentVectorEntry* APS5_VABI _ZNK7WebCore12SharedBuffer5beginEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
const NKWebKit::WebCoreAbi::DataSegmentVectorEntry* APS5_VABI _ZNK7WebCore12SharedBuffer3endEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
const char* APS5_VABI _ZNK7WebCore12SharedBuffer11DataSegment4dataEv(const NKWebKit::WebCoreAbi::DataSegment* self);
std::size_t APS5_VABI _ZNK7WebCore12SharedBuffer11DataSegment4sizeEv(const NKWebKit::WebCoreAbi::DataSegment* self);

}
