#pragma once

#include "prx/libSceNKWebKit/include/ComponentTransferFilterExports.hpp"

namespace NKWebKit {

using ColorMatrixFilterOperationAbi = ComponentTransferFilterOperationAbi;

}  // namespace NKWebKit

extern "C" {

bool APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(const NKWebKit::ColorMatrixFilterOperationAbi* self, WebCore::SRGBA<float>* color);

}
