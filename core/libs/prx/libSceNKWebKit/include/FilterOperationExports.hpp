#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/FilterOperationAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

extern "C" {

NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(WebCore::Length* stdDeviation);
void APS5_VABI _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(WebCore::BlurFilterOperation* self, WebCore::Length* stdDeviation);
void APS5_VABI _ZN7WebCore19BlurFilterOperationC2ENS_6LengthE(WebCore::BlurFilterOperation* self, WebCore::Length* stdDeviation);
void APS5_VABI _ZN7WebCore19BlurFilterOperationD0Ev(WebCore::BlurFilterOperation* self);
void APS5_VABI _ZN7WebCore19BlurFilterOperationD1Ev(WebCore::BlurFilterOperation* self);
void APS5_VABI _ZN7WebCore19BlurFilterOperationD2Ev(WebCore::BlurFilterOperation* self);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(const WebCore::IntPoint* location, int stdDeviation, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(WebCore::DropShadowFilterOperation* self, const WebCore::IntPoint* location, int stdDeviation, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE(WebCore::DropShadowFilterOperation* self, const WebCore::IntPoint* location, int stdDeviation, const WebCore::Color* color);
void APS5_VABI _ZN7WebCore25DropShadowFilterOperationD0Ev(WebCore::DropShadowFilterOperation* self);
void APS5_VABI _ZN7WebCore25DropShadowFilterOperationD1Ev(WebCore::DropShadowFilterOperation* self);
void APS5_VABI _ZN7WebCore25DropShadowFilterOperationD2Ev(WebCore::DropShadowFilterOperation* self);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZN7WebCore30InvertLightnessFilterOperation6createEv();
void APS5_VABI _ZN7WebCore30InvertLightnessFilterOperationC1Ev(WebCore::InvertLightnessFilterOperation* self);
void APS5_VABI _ZN7WebCore30InvertLightnessFilterOperationC2Ev(WebCore::InvertLightnessFilterOperation* self);
NKWebKit::WtfAbi::TextStream* APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE(NKWebKit::WtfAbi::TextStream* stream, const WebCore::FilterOperation* operation);
NKWebKit::WtfAbi::TextStream* APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(NKWebKit::WtfAbi::TextStream* stream, const NKWebKit::FilterAbi::FilterOperations* operations);
const WebCore::Length* APS5_VABI _ZNK7WebCore19BlurFilterOperation12stdDeviationEv(const WebCore::BlurFilterOperation* self);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZNK7WebCore19BlurFilterOperation5cloneEv(const WebCore::BlurFilterOperation* self);
bool APS5_VABI _ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(const WebCore::BlurFilterOperation* self, const WebCore::FilterOperation* other);
WebCore::FilterOperationType APS5_VABI _ZNK7WebCore22DefaultFilterOperation15representedTypeEv(const WebCore::DefaultFilterOperation* self);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZNK7WebCore22DefaultFilterOperation5cloneEv(const WebCore::DefaultFilterOperation* self);
bool APS5_VABI _ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(const WebCore::DefaultFilterOperation* self, const WebCore::FilterOperation* other);
int APS5_VABI _ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv(const WebCore::DropShadowFilterOperation* self);
int APS5_VABI _ZNK7WebCore25DropShadowFilterOperation1xEv(const WebCore::DropShadowFilterOperation* self);
int APS5_VABI _ZNK7WebCore25DropShadowFilterOperation1yEv(const WebCore::DropShadowFilterOperation* self);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZNK7WebCore25DropShadowFilterOperation5cloneEv(const WebCore::DropShadowFilterOperation* self);
const WebCore::Color* APS5_VABI _ZNK7WebCore25DropShadowFilterOperation5colorEv(const WebCore::DropShadowFilterOperation* self);
WebCore::IntPoint APS5_VABI _ZNK7WebCore25DropShadowFilterOperation8locationEv(const WebCore::DropShadowFilterOperation* self);
bool APS5_VABI _ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(const WebCore::DropShadowFilterOperation* self, const WebCore::FilterOperation* other);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperation5cloneEv(const WebCore::InvertLightnessFilterOperation* self);
bool APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(const WebCore::InvertLightnessFilterOperation* self, const WebCore::FilterOperation* other);
double APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(const WebCore::BasicColorMatrixFilterOperation* self);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv(const WebCore::BasicColorMatrixFilterOperation* self);
double APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation6amountEv(const WebCore::BasicColorMatrixFilterOperation* self);
bool APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(const WebCore::BasicColorMatrixFilterOperation* self, const WebCore::FilterOperation* other);
bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(const WebCore::BasicComponentTransferFilterOperation* self);
double APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(const WebCore::BasicComponentTransferFilterOperation* self);
NKWebKit::FilterAbi::FilterOperationRef APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv(const WebCore::BasicComponentTransferFilterOperation* self);
double APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation6amountEv(const WebCore::BasicComponentTransferFilterOperation* self);
bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(const WebCore::BasicComponentTransferFilterOperation* self, const WebCore::FilterOperation* other);

}
