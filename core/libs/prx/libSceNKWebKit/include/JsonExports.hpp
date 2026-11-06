#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfJsonTypes.hpp"
#include <cstddef>

extern "C" {

NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Array> APS5_VABI _ZN3WTF8JSONImpl5Array6createEv();
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value> APS5_VABI _ZN3WTF8JSONImpl5Value4nullEv();
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(const NKWebKit::WtfAbi::String& value);
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createEb(bool value);
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createEd(double value);
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createEi(int value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC1ENS1_4TypeE(NKWebKit::JsonAbi::Value* self, NKWebKit::JsonAbi::ValueType type);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC1ERKNS_6StringE(NKWebKit::JsonAbi::Value* self, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Eb(NKWebKit::JsonAbi::Value* self, bool value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Ed(NKWebKit::JsonAbi::Value* self, double value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Ei(NKWebKit::JsonAbi::Value* self, int value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Ev(NKWebKit::JsonAbi::Value* self);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC2ENS1_4TypeE(NKWebKit::JsonAbi::Value* self, NKWebKit::JsonAbi::ValueType type);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC2ERKNS_6StringE(NKWebKit::JsonAbi::Value* self, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Eb(NKWebKit::JsonAbi::Value* self, bool value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Ed(NKWebKit::JsonAbi::Value* self, double value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Ei(NKWebKit::JsonAbi::Value* self, int value);
void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Ev(NKWebKit::JsonAbi::Value* self);
void APS5_VABI _ZN3WTF8JSONImpl5ValueD0Ev(NKWebKit::JsonAbi::Value* self);
void APS5_VABI _ZN3WTF8JSONImpl5ValueD1Ev(NKWebKit::JsonAbi::Value* self);
void APS5_VABI _ZN3WTF8JSONImpl5ValueD2Ev(NKWebKit::JsonAbi::Value* self);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase10pushDoubleEd(NKWebKit::JsonAbi::ArrayBase* self, double value);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase10pushStringERKNS_6StringE(NKWebKit::JsonAbi::ArrayBase* self, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase11pushBooleanEb(NKWebKit::JsonAbi::ArrayBase* self, bool value);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(NKWebKit::JsonAbi::ArrayBase* self, int value);
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value>* APS5_VABI _ZN3WTF8JSONImpl9ArrayBase3endEv(NKWebKit::JsonAbi::ArrayBase* self);
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value>* APS5_VABI _ZN3WTF8JSONImpl9ArrayBase5beginEv(NKWebKit::JsonAbi::ArrayBase* self);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseC1Ev(NKWebKit::JsonAbi::ArrayBase* self);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseC2Ev(NKWebKit::JsonAbi::ArrayBase* self);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseD0Ev(NKWebKit::JsonAbi::ArrayBase* self);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseD1Ev(NKWebKit::JsonAbi::ArrayBase* self);
std::size_t APS5_VABI _ZNK3WTF8JSONImpl5Value10memoryCostEv(const NKWebKit::JsonAbi::Value* self);
NKWebKit::JsonAbi::ValueType APS5_VABI _ZNK3WTF8JSONImpl5Value4typeEv(const NKWebKit::JsonAbi::Value* self);
bool APS5_VABI _ZNK3WTF8JSONImpl5Value6isNullEv(const NKWebKit::JsonAbi::Value* self);
bool APS5_VABI _ZNK3WTF8JSONImpl5Value8asDoubleERd(const NKWebKit::JsonAbi::Value* self, double& output);
bool APS5_VABI _ZNK3WTF8JSONImpl5Value9asIntegerERi(const NKWebKit::JsonAbi::Value* self, int& output);
std::size_t APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase10memoryCostEv(const NKWebKit::JsonAbi::ArrayBase* self);
NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value> APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase3getEm(const NKWebKit::JsonAbi::ArrayBase* self, std::size_t index);
const NKWebKit::JsonAbi::Ref<NKWebKit::JsonAbi::Value>* APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase5beginEv(const NKWebKit::JsonAbi::ArrayBase* self);

}
