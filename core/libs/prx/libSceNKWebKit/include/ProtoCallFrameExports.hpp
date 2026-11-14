#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ProtoCallFrameAbi.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

int APS5_VABI _ZNK3JSC14ProtoCallFrame13argumentCountEv(const NKWebKit::JscAbi::ProtoCallFrame* self);
std::uint64_t APS5_VABI _ZN3JSC14ProtoCallFrame8argumentEm(NKWebKit::JscAbi::ProtoCallFrame* self, std::size_t argumentIndex);
void APS5_VABI _ZN3JSC14ProtoCallFrame11setArgumentEmNS_7JSValueE(NKWebKit::JscAbi::ProtoCallFrame* self, std::size_t argumentIndex, std::uint64_t value);
void APS5_VABI _ZN3JSC14ProtoCallFrame15clearCurrentVPCEv(NKWebKit::JscAbi::ProtoCallFrame* self);
NKWebKit::JscAbi::ProtoCallFrame& APS5_VABI _ZN3JSC14ProtoCallFrameaSERKS0_(NKWebKit::JscAbi::ProtoCallFrame* self, const NKWebKit::JscAbi::ProtoCallFrame& other);

}
