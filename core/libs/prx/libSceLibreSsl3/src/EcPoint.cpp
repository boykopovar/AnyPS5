#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace {

constexpr int PointConversionUncompressed = 4;

struct PointDeleter {
    void operator()(EcPoint* point) const {
        EC_POINT_free_nid_postfix(point);
    }
};

} // namespace

extern "C" {

EcPoint* APS5_VABI EC_POINT_dup_nid_postfix(const EcPoint* source, const EcGroup* group) {
    if (source == nullptr || group == nullptr) return nullptr;
    const std::size_t size = EC_POINT_point2oct_nid_postfix(group, source, PointConversionUncompressed, nullptr, 0, nullptr);
    if (size == 0) return nullptr;
    std::unique_ptr<EcPoint, PointDeleter> copy(EC_POINT_new_nid_postfix(group));
    if (!copy) return nullptr;
    std::vector<unsigned char> encoded(size);
    if (EC_POINT_point2oct_nid_postfix(group, source, PointConversionUncompressed, encoded.data(), encoded.size(), nullptr) != size) return nullptr;
    if (EC_POINT_oct2point_nid_postfix(group, copy.get(), encoded.data(), encoded.size(), nullptr) != 1) return nullptr;
    return copy.release();
}

}
