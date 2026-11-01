#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKDataCreate(const unsigned char* bytes, std::size_t size) {
    std::vector<std::uint8_t> data;
    if (bytes && size > 0) {
        data.assign(bytes, bytes + size);
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Data>(std::move(data)));
}

const unsigned char* APS5_VABI WKDataGetBytes(const void* dataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::Data>(dataRef, __func__);
    return data->bytes.data();
}

std::size_t APS5_VABI WKDataGetSize(const void* dataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::Data>(dataRef, __func__);
    return data->bytes.size();
}

}
