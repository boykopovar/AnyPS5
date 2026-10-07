#include "Optimization/DescriptorBindingBuilder.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace ShaderRecompiler {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

// Debug aid: APS5_TRACE_BUFFER_WRITTEN=1 prints, after every Populate, how many guest buffer
// elements the program may store to and how many it only loads (this call and cumulative), so the
// replay tool shows what a driver gains from DescriptorBinding::bufferWritten.
bool bufferWrittenTraceEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_BUFFER_WRITTEN") != nullptr;
    return enabled;
}
struct BufferWrittenCounts {
    std::atomic<unsigned long long> written{0};
    std::atomic<unsigned long long> readOnly{0};
};
BufferWrittenCounts bufferWrittenCounts;

DescriptorKind PhysicalKindFor(DescriptorBindingKind kind) {
    if (kind == DescriptorBindingKind::Samplers) {
        return DescriptorKind::Sampler;
    }
    const ImageResourceClass imageClass = ImageBindingResourceClass(kind);
    if (imageClass == ImageResourceClass::Sampled) {
        return DescriptorKind::SampledImage;
    }
    if (imageClass == ImageResourceClass::Storage) {
        return DescriptorKind::StorageImage;
    }
    return DescriptorKind::StorageBuffer;
}

DescriptorRole RoleFor(DescriptorBindingKind kind) {
    if (kind == DescriptorBindingKind::Buffers) {
        return DescriptorRole::GuestBuffers;
    }
    if (kind == DescriptorBindingKind::Samplers) {
        return DescriptorRole::GuestSamplers;
    }
    if (kind == DescriptorBindingKind::Gds) {
        return DescriptorRole::Gds;
    }
    if (kind == DescriptorBindingKind::BdaPagetable) {
        return DescriptorRole::BdaPagetable;
    }
    if (kind == DescriptorBindingKind::FaultBuffer) {
        return DescriptorRole::FaultBuffer;
    }
    if (kind == DescriptorBindingKind::FlattenedSrt) {
        return DescriptorRole::FlattenedSrt;
    }
    if (kind == DescriptorBindingKind::ShaderData) {
        return DescriptorRole::ShaderData;
    }
    if (ImageBindingResourceClass(kind) != ImageResourceClass::None) {
        return DescriptorRole::GuestImages;
    }
    fail("DescriptorBindingBuilder::Populate binding kind has no descriptor role");
}

DescriptorImageShape ImageShapeForResource(const ImageResource& image) {
    const RdnaImageDimensionInfo& info = RdnaImageDimensionInfoFor(image.dimension);
    if (info.multisampled != 0u) {
        fail("DescriptorBindingBuilder::Populate multisampled image resources have no descriptor image shape");
    }
    if (info.spirvDimension == spv::Dim1D) {
        return info.arrayed != 0u ? DescriptorImageShape::Image1DArray : DescriptorImageShape::Image1D;
    }
    if (info.spirvDimension == spv::Dim3D) {
        return DescriptorImageShape::Image3D;
    }
    if (info.spirvDimension == spv::Dim2D) {
        // Cube images are declared and addressed as 2D arrays of faces (the backend converts
        // cube coordinates to face layers), so they bind as 2D arrays.
        if (image.cube && info.arrayed == 0u) {
            fail("DescriptorBindingBuilder::Populate cube image resource is not arrayed");
        }
        return info.arrayed != 0u ? DescriptorImageShape::Image2DArray : DescriptorImageShape::Image2D;
    }
    fail("DescriptorBindingBuilder::Populate image resource dimension has no descriptor image shape");
}

DescriptorImageShape ImageShapeFor(const std::vector<ImageResource>& images, const std::vector<std::uint32_t>& resources) {
    if (resources.empty()) {
        fail("DescriptorBindingBuilder::Populate guest image binding has no resources");
    }
    std::optional<DescriptorImageShape> shape;
    for (const std::uint32_t r : resources) {
        const DescriptorImageShape current = ImageShapeForResource(images.at(r));
        if (shape.has_value() && *shape != current) {
            fail("DescriptorBindingBuilder::Populate guest image array elements disagree on image shape");
        }
        shape = current;
    }
    return *shape;
}

std::vector<std::uint32_t> GuestBuffersDescriptor(const std::vector<std::uint32_t>& resources, const ResourceSnapshot& snapshot) {
    std::vector<std::uint32_t> result;
    result.reserve(resources.size() * 4u);
    for (const std::uint32_t r : resources) {
        if (r >= snapshot.buffers.size()) {
            fail("DescriptorBindingBuilder::Populate guest buffer index is out of range");
        }
        const DescriptorValue& value = snapshot.buffers[r];
        if (value.dwordCount != 4u) {
            fail("DescriptorBindingBuilder::Populate guest buffer descriptor has an invalid width");
        }
        for (std::uint32_t dword = 0; dword < 4u; dword++) {
            result.push_back(value.dwords[dword]);
        }
    }
    return result;
}

std::vector<std::uint32_t> GuestImagesDescriptor(const std::vector<std::uint32_t>& resources, const ResourceSnapshot& snapshot) {
    std::vector<std::uint32_t> result;
    std::uint32_t dwordCount = 0;
    for (std::size_t i = 0; i < resources.size(); i++) {
        const std::uint32_t r = resources[i];
        if (r >= snapshot.images.size()) {
            fail("DescriptorBindingBuilder::Populate guest image index is out of range");
        }
        const DescriptorValue& value = snapshot.images[r];
        if (value.dwordCount == 0u) {
            fail("DescriptorBindingBuilder::Populate guest image descriptor is empty");
        }
        if (i == 0u) {
            dwordCount = value.dwordCount;
        } else if (value.dwordCount != dwordCount) {
            fail("DescriptorBindingBuilder::Populate guest image descriptors have inconsistent widths");
        }
        for (std::uint32_t dword = 0; dword < value.dwordCount; dword++) {
            result.push_back(value.dwords[dword]);
        }
    }
    return result;
}

std::vector<std::uint32_t> GuestSamplersDescriptor(const std::vector<std::uint32_t>& resources, const ResourceSnapshot& snapshot) {
    std::vector<std::uint32_t> result;
    std::uint32_t dwordCount = 0;
    for (std::size_t i = 0; i < resources.size(); i++) {
        const std::uint32_t r = resources[i];
        if (r >= snapshot.samplers.size()) {
            fail("DescriptorBindingBuilder::Populate guest sampler index is out of range");
        }
        const DescriptorValue& value = snapshot.samplers[r];
        if (value.dwordCount == 0u) {
            fail("DescriptorBindingBuilder::Populate guest sampler descriptor is empty");
        }
        if (i == 0u) {
            dwordCount = value.dwordCount;
        } else if (value.dwordCount != dwordCount) {
            fail("DescriptorBindingBuilder::Populate guest sampler descriptors have inconsistent widths");
        }
        for (std::uint32_t dword = 0; dword < value.dwordCount; dword++) {
            result.push_back(value.dwords[dword]);
        }
    }
    return result;
}

constexpr std::uint32_t ForceUnnormalizedBit = 1u << 15u;

[[noreturn]] void failUnnormalized(const char* reason) {
    fail(std::string("DescriptorBindingBuilder::Populate unnormalized guest sampler ") + reason + ", which is not implemented");
}

const char* UnnormalizedUseReason(std::uint32_t uses) {
    switch (uses & (~uses + 1u)) {
        case SamplerUseImplicitLod: return "is used by an implicit-LOD sample";
        case SamplerUseGradient: return "is used by a sample with derivatives";
        case SamplerUseOffset: return "is used with a texel offset";
        case SamplerUseCompare: return "is used with depth comparison";
        case SamplerUseGather: return "is used by a gather";
        case SamplerUseQueryLod: return "is used by image_get_lod";
        default: return "is used by an image_sample_*_a variant";
    }
}

struct UnnormalizedProof {
    std::vector<bool> samplers;
    std::vector<bool> images;
};

UnnormalizedProof ProveUnnormalized(const ShaderInfo& info, const ResourceSnapshot& snapshot) {
    UnnormalizedProof proof{std::vector<bool>(info.samplers.size()), std::vector<bool>(info.images.size())};
    for (std::uint32_t r = 0; r < info.samplers.size(); r++) {
        if ((GuestSamplersDescriptor({r}, snapshot)[0] & ForceUnnormalizedBit) == 0u) {
            continue;
        }
        const auto& sampler = info.samplers[r];
        const std::uint32_t unsupported = sampler.uses & ~static_cast<std::uint32_t>(SamplerUseExplicitLod);
        if (unsupported != 0u) {
            failUnnormalized(UnnormalizedUseReason(unsupported));
        }
        if (sampler.depthCompare) {
            failUnnormalized("is used with depth comparison");
        }
        for (const auto& pair : info.sampledPairs) {
            if (pair.sampler != r) {
                continue;
            }
            const auto& image = info.images.at(pair.image);
            if (image.indirectRoot != ImageResource::NoIndirectImage) {
                failUnnormalized("samples an image selected at run time");
            }
            if ((image.dimension != RdnaImageDimension::Dim1D && image.dimension != RdnaImageDimension::Dim2D) || image.cube) {
                failUnnormalized("samples a 1D-array, 2D-array, 3D, cube or multisampled image");
            }
            if (image.depthCompare) {
                failUnnormalized("is used with depth comparison");
            }
            if (image.conversionFormat != IrBufferFormat::Invalid || image.packed) {
                failUnnormalized("samples an image that needs a format conversion or packed access");
            }
            proof.images[pair.image] = true;
        }
        proof.samplers[r] = true;
    }
    return proof;
}

std::vector<std::uint32_t> SamplerElements(const IrBindingLayout& layout, const ShaderInfo& info) {
    std::vector<std::uint32_t> elements(info.samplers.size(), ShaderInfo::MaxSamplers);
    for (const IrDescriptorBinding& logical : layout.descriptors) {
        if (logical.kind != DescriptorBindingKind::Samplers) {
            continue;
        }
        for (std::uint32_t element = 0; element < logical.resources.size() && element < ShaderInfo::MaxSamplers; element++) {
            elements.at(logical.resources[element]) = element;
        }
    }
    return elements;
}

std::uint32_t ImageSamplerMask(const ShaderInfo& info, const std::vector<std::uint32_t>& samplerElements, std::uint32_t resource) {
    const std::uint32_t root = info.images.at(resource).indirectRoot;
    std::uint32_t mask = 0;
    for (const SampledResourcePair& pair : info.sampledPairs) {
        if (pair.image != resource && pair.image != root) {
            continue;
        }
        if (pair.sampler >= samplerElements.size() || samplerElements[pair.sampler] >= ShaderInfo::MaxSamplers) {
            fail("DescriptorBindingBuilder::Populate sampled image pair names a sampler outside the first " + std::to_string(ShaderInfo::MaxSamplers) + " elements of the sampler binding");
        }
        mask |= 1u << samplerElements[pair.sampler];
    }
    return mask;
}

std::vector<std::uint32_t> ShaderDataDwordsFor(const IrBindingLayout& layout, std::uint32_t userDataBase, const ResourceSnapshot& snapshot, const std::array<std::uint32_t, 3>& partialThreads) {
    std::vector<std::uint32_t> result(layout.ShaderDataDwords(), 0u);
    for (std::size_t i = 0; i < layout.userDataRegisters.size(); i++) {
        const std::uint32_t reg = layout.userDataRegisters[i];
        if (reg < userDataBase || reg - userDataBase >= snapshot.userData.size()) {
            fail("DescriptorBindingBuilder::Populate user-data register is out of range");
        }
        result[i] = snapshot.userData[reg - userDataBase];
    }
    if (layout.dispatchThreadLimit) {
        if (partialThreads == std::array<std::uint32_t, 3>{}) {
            fail("DescriptorBindingBuilder::Populate partial-group shader has no dispatch size");
        }
        std::copy(partialThreads.begin(), partialThreads.end(), result.begin() + layout.DispatchThreadLimitDword());
    }
    return result;
}

}

std::uint32_t PointFilteredSamplerWord(std::uint32_t word0, std::uint32_t filter) {
    const bool reduced = ((word0 >> 29u) & 3u) != 0u;
    if (reduced && (((filter >> 20u) & 0xfu) != 0u || ((filter >> 26u) & 3u) == 2u)) {
        fail("DescriptorBindingBuilder: a min or max reduction sampler that filters between texels or mip levels samples an image that needs point filtering (sint, converted or depth-bits format), which is not implemented");
    }
    const bool mipmapped = ((filter >> 26u) & 3u) != 0u;
    return (filter & ~(0xffu << 20u)) | (1u << 24u) | (mipmapped ? 1u << 26u : 0u);
}

void DescriptorBindingBuilder::Populate(BindingAllocationResult& allocation, const IrProgram& program, const ResourceSnapshot& snapshot, const std::array<std::uint32_t, 3>& partialThreads) const {
    Populate(allocation, program.Info(), program.Resources().stage, program.Resources().userDataBase, snapshot, partialThreads);
}

void DescriptorBindingBuilder::Populate(BindingAllocationResult& allocation, const ShaderInfo& info, IrShaderStage stage, std::uint32_t userDataBase, const ResourceSnapshot& snapshot, const std::array<std::uint32_t, 3>& partialThreads) const {
    const IrBindingLayout& layout = allocation.layout;
    const std::vector<std::uint32_t> shaderData = ShaderDataDwordsFor(layout, userDataBase, snapshot, partialThreads);
    const UnnormalizedProof unnormalized = ProveUnnormalized(info, snapshot);
    const std::vector<std::uint32_t> samplerElements = SamplerElements(layout, info);

    std::vector<DescriptorBinding> bindings;
    bindings.reserve(layout.descriptors.size());
    std::size_t writtenHere = 0;
    std::size_t readOnlyHere = 0;
    for (const IrDescriptorBinding& logical : layout.descriptors) {
        DescriptorBinding physical;
        physical.descriptorSet = 0u;
        physical.binding = NativeBinding(stage, logical.kind);
        physical.count = logical.resources.empty() ? 1u : static_cast<std::uint32_t>(logical.resources.size());
        physical.kind = PhysicalKindFor(logical.kind);
        physical.role = RoleFor(logical.kind);
        physical.readOnly = false;

        switch (physical.role) {
        case DescriptorRole::GuestBuffers:
            physical.guestDescriptor = GuestBuffersDescriptor(logical.resources, snapshot);
            for (const std::uint32_t resource : logical.resources) {
                const auto& buffer = info.buffers.at(resource);
                physical.bufferAtomic.push_back(buffer.atomic);
                // The tracker merges every buffer access of a source into its resource
                // (ResourceTracker::Merge), so an element without a store or atomic is read-only
                // over its whole extent; stores through pointers (BDA) never bind a V#.
                physical.bufferWritten.push_back(buffer.written || buffer.atomic);
                if (buffer.written || buffer.atomic) ++writtenHere;
                else ++readOnlyHere;
            }
            break;
        case DescriptorRole::GuestImages:
            physical.guestDescriptor = GuestImagesDescriptor(logical.resources, snapshot);
            physical.imageShape = ImageShapeFor(info.images, logical.resources);
            for (const std::uint32_t resource : logical.resources) {
                const auto& image = info.images.at(resource);
                physical.imageWritten.push_back(image.written || image.atomic);
                physical.imageDepthCompare.push_back(image.depthCompare);
                physical.imageAtomic.push_back(image.atomic);
                physical.imageAtomic64.push_back(image.atomic64);
                physical.imageUnnormalized.push_back(unnormalized.images.at(resource));
                physical.imageSamplers.push_back(ImageSamplerMask(info, samplerElements, resource));
            }
            break;
        case DescriptorRole::GuestSamplers:
            physical.guestDescriptor = GuestSamplersDescriptor(logical.resources, snapshot);
            for (std::size_t element = 0; element < logical.resources.size(); ++element) {
                const auto& sampler = info.samplers.at(logical.resources[element]);
                physical.samplerDepthCompare.push_back(sampler.depthCompare);
                physical.samplerUnnormalized.push_back(unnormalized.samplers.at(logical.resources[element]));
                if (sampler.forcePointFiltering) {
                    auto& filter = physical.guestDescriptor.at(element * 4u + 2u);
                    filter = PointFilteredSamplerWord(physical.guestDescriptor.at(element * 4u), filter);
                }
            }
            break;
        case DescriptorRole::FlattenedSrt:
            if (snapshot.flattenedSrt.empty()) {
                fail("DescriptorBindingBuilder::Populate flattened SRT snapshot is empty");
            }
            physical.guestDescriptor = snapshot.flattenedSrt;
            break;
        case DescriptorRole::ShaderData:
            if (layout.UsesPushData()) {
                fail("DescriptorBindingBuilder::Populate shader-data binding must not exist when push data is used");
            }
            physical.guestDescriptor = shaderData;
            break;
        case DescriptorRole::Gds:
        case DescriptorRole::BdaPagetable:
        case DescriptorRole::FaultBuffer:
            break;
        }

        if (physical.role == DescriptorRole::GuestBuffers || physical.role == DescriptorRole::GuestImages || physical.role == DescriptorRole::GuestSamplers) {
            if (physical.count == 0u || physical.guestDescriptor.size() % physical.count != 0u) {
                fail("DescriptorBindingBuilder::Populate guest descriptor size is not a multiple of the binding count");
            }
        }

        bindings.push_back(std::move(physical));
    }

    if (bufferWrittenTraceEnabled()) {
        // Cumulative counts include every Populate of the process (the replay tool populates
        // each program twice), so the per-call counts are the ones to sum per program.
        const auto written = bufferWrittenCounts.written.fetch_add(writtenHere) + writtenHere;
        const auto readOnly = bufferWrittenCounts.readOnly.fetch_add(readOnlyHere) + readOnlyHere;
        std::fprintf(stderr, "[bindings] guest buffer elements: %zu written, %zu read-only (total so far: %llu / %llu)\n", writtenHere, readOnlyHere, written, readOnly);
    }
    allocation.bindings = std::move(bindings);
    allocation.pushConstants.clear();
    if (layout.UsesPushData()) {
        allocation.pushConstants.resize(static_cast<std::size_t>(shaderData.size()) * sizeof(std::uint32_t));
        std::memcpy(allocation.pushConstants.data(), shaderData.data(), allocation.pushConstants.size());
    }
}

}
