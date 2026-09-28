#include "Optimization/DescriptorBindingBuilder.hpp"
#include "SpirvBackend/SpirvEmitterHelpers.hpp"
#include <spirv/unified1/spirv.hpp>
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
        if (info.arrayed != 0u) {
            fail("DescriptorBindingBuilder::Populate 1D array image resources have no descriptor image shape");
        }
        return DescriptorImageShape::Image1D;
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

std::vector<std::uint32_t> ShaderDataDwordsFor(const IrBindingLayout& layout, std::uint32_t userDataBase, const ResourceSnapshot& snapshot) {
    std::vector<std::uint32_t> result(layout.ShaderDataDwords(), 0u);
    for (std::size_t i = 0; i < layout.userDataRegisters.size(); i++) {
        const std::uint32_t reg = layout.userDataRegisters[i];
        if (reg < userDataBase || reg - userDataBase >= snapshot.userData.size()) {
            fail("DescriptorBindingBuilder::Populate user-data register is out of range");
        }
        result[i] = snapshot.userData[reg - userDataBase];
    }
    return result;
}

}

void DescriptorBindingBuilder::Populate(BindingAllocationResult& allocation, const IrProgram& program, const ResourceSnapshot& snapshot) const {
    Populate(allocation, program.Info(), program.Resources().stage, program.Resources().userDataBase, snapshot);
}

void DescriptorBindingBuilder::Populate(BindingAllocationResult& allocation, const ShaderInfo& info, IrShaderStage stage, std::uint32_t userDataBase, const ResourceSnapshot& snapshot) const {
    const IrBindingLayout& layout = allocation.layout;
    const std::vector<std::uint32_t> shaderData = ShaderDataDwordsFor(layout, userDataBase, snapshot);

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
            }
            break;
        case DescriptorRole::GuestSamplers:
            physical.guestDescriptor = GuestSamplersDescriptor(logical.resources, snapshot);
            for (std::size_t element = 0; element < logical.resources.size(); ++element) {
                const auto& sampler = info.samplers.at(logical.resources[element]);
                physical.samplerDepthCompare.push_back(sampler.depthCompare);
                if (sampler.forcePointFiltering) {
                    auto& filter = physical.guestDescriptor.at(element * 4u + 2u);
                    const bool mipmapped = ((filter >> 26u) & 3u) != 0u;
                    filter = (filter & ~(0xffu << 20u)) | (1u << 24u) | (mipmapped ? 1u << 26u : 0u);
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
