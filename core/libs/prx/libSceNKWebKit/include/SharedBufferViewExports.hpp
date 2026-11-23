#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SharedBufferExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct SharedBufferDataView {
    std::size_t positionWithinSegment;
    DataSegment* segment;
};

static_assert(std::is_standard_layout_v<SharedBufferDataView>);
static_assert(sizeof(SharedBufferDataView) == 16);
static_assert(offsetof(SharedBufferDataView, segment) == 8);

struct SharedBufferDataViewResult {
    SharedBufferDataView view;
    SharedBufferDataViewResult(const SharedBufferDataViewResult&) = delete;
    SharedBufferDataViewResult& operator=(const SharedBufferDataViewResult&) = delete;
    explicit SharedBufferDataViewResult(SharedBufferDataView value) : view(value) {}
    SharedBufferDataViewResult(SharedBufferDataViewResult&& other) noexcept : view(other.view) { other.view.segment = nullptr; }
    ~SharedBufferDataViewResult();
};

static_assert(sizeof(SharedBufferDataViewResult) == 16);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer24createWithContentsOfFileERKN3WTF6StringE(const NKWebKit::WtfAbi::String* filePath);
NKWebKit::WebCoreAbi::SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer21createFromReadingFileERKN3WTF6StringE(const NKWebKit::WtfAbi::String* filePath);
NKWebKit::WebCoreAbi::SharedBufferDataViewResult APS5_VABI _ZNK7WebCore12SharedBuffer11getSomeDataEm(const NKWebKit::WebCoreAbi::SharedBuffer* self, std::size_t position);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore12SharedBuffer11toHexStringEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
void APS5_VABI _ZNK7WebCore12SharedBuffer23hintMemoryNotNeededSoonEv(const NKWebKit::WebCoreAbi::SharedBuffer* self);
bool APS5_VABI _ZNK7WebCore12SharedBuffereqERKS0_(const NKWebKit::WebCoreAbi::SharedBuffer* self, const NKWebKit::WebCoreAbi::SharedBuffer* other);
bool APS5_VABI _ZNK7WebCore12SharedBufferneERKS0_(const NKWebKit::WebCoreAbi::SharedBuffer* self, const NKWebKit::WebCoreAbi::SharedBuffer* other);
void APS5_VABI _ZN7WebCore20SharedBufferDataViewC1EON3WTF3RefINS_12SharedBuffer11DataSegmentENS1_13DumbPtrTraitsIS4_EEEEm(NKWebKit::WebCoreAbi::SharedBufferDataView* self, NKWebKit::WebCoreAbi::DataSegment** segment, std::size_t positionWithinSegment);
void APS5_VABI _ZN7WebCore20SharedBufferDataViewC2EON3WTF3RefINS_12SharedBuffer11DataSegmentENS1_13DumbPtrTraitsIS4_EEEEm(NKWebKit::WebCoreAbi::SharedBufferDataView* self, NKWebKit::WebCoreAbi::DataSegment** segment, std::size_t positionWithinSegment);
void APS5_VABI _ZN7WebCore20SharedBufferDataViewC1ERKS0_(NKWebKit::WebCoreAbi::SharedBufferDataView* self, const NKWebKit::WebCoreAbi::SharedBufferDataView* other);
void APS5_VABI _ZN7WebCore20SharedBufferDataViewC2ERKS0_(NKWebKit::WebCoreAbi::SharedBufferDataView* self, const NKWebKit::WebCoreAbi::SharedBufferDataView* other);
void APS5_VABI _ZN7WebCore20SharedBufferDataViewD1Ev(NKWebKit::WebCoreAbi::SharedBufferDataView* self);
void APS5_VABI _ZN7WebCore20SharedBufferDataViewD2Ev(NKWebKit::WebCoreAbi::SharedBufferDataView* self);
NKWebKit::WebCoreAbi::SharedBufferDataView* APS5_VABI _ZN7WebCore20SharedBufferDataViewaSERKS0_(NKWebKit::WebCoreAbi::SharedBufferDataView* self, const NKWebKit::WebCoreAbi::SharedBufferDataView* other);
const char* APS5_VABI _ZNK7WebCore20SharedBufferDataView4dataEv(const NKWebKit::WebCoreAbi::SharedBufferDataView* self);
std::size_t APS5_VABI _ZNK7WebCore20SharedBufferDataView4sizeEv(const NKWebKit::WebCoreAbi::SharedBufferDataView* self);

}
