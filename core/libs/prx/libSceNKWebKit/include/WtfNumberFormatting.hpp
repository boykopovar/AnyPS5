#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF6String24numberToStringFixedWidthEdj(double number, unsigned decimalPlaces);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF6String28numberToStringFixedPrecisionEdjNS_29TrailingZerosTruncatingPolicyE(double number, unsigned precision, int policy);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF6String28numberToStringFixedPrecisionEfjNS_29TrailingZerosTruncatingPolicyE(float number, unsigned precision, int policy);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF6String6numberEd(double number);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF6String6numberEf(float number);

}
