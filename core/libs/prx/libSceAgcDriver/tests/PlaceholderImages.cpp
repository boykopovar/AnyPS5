#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

VkImageViewType ViewType(RdnaImageDimension dimension) {
    switch (dimension) {
        case RdnaImageDimension::Dim1D: return VK_IMAGE_VIEW_TYPE_1D;
        case RdnaImageDimension::Dim1DArray: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case RdnaImageDimension::Dim2D:
        case RdnaImageDimension::Dim2DMsaa: return VK_IMAGE_VIEW_TYPE_2D;
        case RdnaImageDimension::Dim2DArray:
        case RdnaImageDimension::Dim2DMsaaArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case RdnaImageDimension::Dim3D: return VK_IMAGE_VIEW_TYPE_3D;
        default: throw std::runtime_error("unknown image dimension");
    }
}

VkFormat Format(const ImageResource& image) {
    if (image.depthCompare) return VK_FORMAT_D16_UNORM;
    if (image.atomic64) return VK_FORMAT_R64_UINT;
    switch (image.numericClass) {
        case IrTextureNumericClass::Float: return VK_FORMAT_R8G8B8A8_UNORM;
        case IrTextureNumericClass::Uint: return VK_FORMAT_R32_UINT;
        case IrTextureNumericClass::Sint: return VK_FORMAT_R32_SINT;
        default: throw std::runtime_error("unknown numeric class");
    }
}

void CheckEveryHeap() {
    constexpr std::array dimensions{RdnaImageDimension::Dim1D, RdnaImageDimension::Dim1DArray, RdnaImageDimension::Dim2D, RdnaImageDimension::Dim2DArray,
                                    RdnaImageDimension::Dim2DMsaa, RdnaImageDimension::Dim2DMsaaArray, RdnaImageDimension::Dim3D};
    std::set<std::uint32_t> heaps;
    for (const auto resourceClass : {ImageResourceClass::Sampled, ImageResourceClass::Storage}) {
        for (const auto numericClass : {IrTextureNumericClass::Float, IrTextureNumericClass::Uint, IrTextureNumericClass::Sint}) {
            for (const bool depthCompare : {false, true}) {
                for (const bool atomic : {false, true}) {
                    for (const bool atomic64 : {false, true}) {
                        if ((depthCompare && resourceClass == ImageResourceClass::Storage) || (atomic64 && !atomic)) continue;
                        for (const auto dimension : dimensions) {
                            ImageResource image;
                            image.resourceClass = resourceClass;
                            image.numericClass = numericClass;
                            image.dimension = dimension;
                            image.depthCompare = depthCompare;
                            image.atomic = atomic;
                            image.atomic64 = atomic64;
                            std::uint32_t heap = 0;
                            try {
                                heap = static_cast<std::uint32_t>(DescriptorBindingForImage(image));
                            } catch (const std::exception&) {
                                continue;
                            }
                            const auto shape = AgcDriver::Graphics::PlaceholderImageShapeOf(heap);
                            const auto where = " for heap " + std::to_string(heap);
                            const bool storage = resourceClass == ImageResourceClass::Storage;
                            const bool multisampled = dimension == RdnaImageDimension::Dim2DMsaa || dimension == RdnaImageDimension::Dim2DMsaaArray;
                            Require(((shape.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) == storage && ((shape.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0) == !storage, "placeholder usage disagrees with the resource class" + where);
                            Require((shape.aspect == VK_IMAGE_ASPECT_DEPTH_BIT) == depthCompare, "placeholder aspect disagrees with depth comparison" + where);
                            Require(shape.format == Format(image), "placeholder format disagrees with the numeric class" + where);
                            Require(shape.viewType == ViewType(dimension), "placeholder view type disagrees with the dimension" + where);
                            Require((shape.samples != VK_SAMPLE_COUNT_1_BIT) == multisampled, "placeholder sample count disagrees with multisampling" + where);
                            Require(((shape.features & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) != 0) == atomic, "placeholder format features disagree with atomics" + where);
                            heaps.insert(heap);
                        }
                    }
                }
            }
        }
    }
    Require(heaps.size() == RuntimeAbi::ImageBindingCount, "not every typed image heap is reachable from an image resource: " + std::to_string(heaps.size()));
}

}

int main() {
    try {
        CheckEveryHeap();
        std::cout << "placeholder image tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
