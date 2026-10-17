#include "Backend.hpp"
#include "DrawStages.hpp"
#include "ShaderCapture.hpp"
#include "WgslBackend/WgslEmitter.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "wasm-runtime/Guest.hpp"
#include <emscripten/eventloop.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <webgpu/webgpu_cpp.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace WasmGpu {
namespace {

using ShaderRecompiler::DescriptorRole;

constexpr std::uint32_t CanvasWidth = 1920;
constexpr std::uint32_t CanvasHeight = 1080;
constexpr std::uint64_t BufferLimit = 64ull << 20u;

void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("WebGPU: " + reason);
}

std::vector<std::byte> ReadGuest(std::uint64_t address, std::uint64_t bytes) {
    Require(address != 0 || bytes == 0, "read of a null guest range");
    std::vector<std::byte> result(static_cast<std::size_t>((bytes + 3u) & ~std::uint64_t{3}));
    std::memcpy(result.data(), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), static_cast<std::size_t>(bytes));
    return result;
}

std::uint64_t Hash(std::uint64_t address, std::uint64_t bytes) {
    const auto* data = reinterpret_cast<const std::byte*>(static_cast<std::uintptr_t>(address));
    std::uint64_t hash = 0xcbf29ce484222325ull ^ bytes;
    std::uint64_t offset = 0;
    for (; offset + 8 <= bytes; offset += 8) {
        std::uint64_t word;
        std::memcpy(&word, data + offset, sizeof(word));
        hash = (hash ^ word) * 0x100000001b3ull;
        hash ^= hash >> 29u;
    }
    for (; offset < bytes; ++offset) hash = (hash ^ static_cast<std::uint64_t>(data[offset])) * 0x100000001b3ull;
    return hash;
}

wgpu::TextureFormat TextureFormat(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM: return wgpu::TextureFormat::RGBA8Unorm;
    case VK_FORMAT_R8G8B8A8_SRGB: return wgpu::TextureFormat::RGBA8UnormSrgb;
    case VK_FORMAT_B8G8R8A8_UNORM: return wgpu::TextureFormat::BGRA8Unorm;
    case VK_FORMAT_B8G8R8A8_SRGB: return wgpu::TextureFormat::BGRA8UnormSrgb;
    case VK_FORMAT_R8_UNORM: return wgpu::TextureFormat::R8Unorm;
    case VK_FORMAT_R8G8_UNORM: return wgpu::TextureFormat::RG8Unorm;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return wgpu::TextureFormat::RGBA16Float;
    case VK_FORMAT_R16_SFLOAT: return wgpu::TextureFormat::R16Float;
    case VK_FORMAT_R16G16_SFLOAT: return wgpu::TextureFormat::RG16Float;
    case VK_FORMAT_R32_SFLOAT: return wgpu::TextureFormat::R32Float;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return wgpu::TextureFormat::RGBA32Float;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return wgpu::TextureFormat::RGB10A2Unorm;
    default: break;
    }
    throw std::runtime_error("WebGPU: VkFormat " + std::to_string(static_cast<int>(format)) + " has no texture format");
}

wgpu::VertexFormat VertexFormat(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R32_SFLOAT: return wgpu::VertexFormat::Float32;
    case VK_FORMAT_R32G32_SFLOAT: return wgpu::VertexFormat::Float32x2;
    case VK_FORMAT_R32G32B32_SFLOAT: return wgpu::VertexFormat::Float32x3;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return wgpu::VertexFormat::Float32x4;
    case VK_FORMAT_R32_UINT: return wgpu::VertexFormat::Uint32;
    case VK_FORMAT_R32G32_UINT: return wgpu::VertexFormat::Uint32x2;
    case VK_FORMAT_R32G32B32_UINT: return wgpu::VertexFormat::Uint32x3;
    case VK_FORMAT_R32G32B32A32_UINT: return wgpu::VertexFormat::Uint32x4;
    case VK_FORMAT_R32_SINT: return wgpu::VertexFormat::Sint32;
    case VK_FORMAT_R32G32_SINT: return wgpu::VertexFormat::Sint32x2;
    case VK_FORMAT_R32G32B32_SINT: return wgpu::VertexFormat::Sint32x3;
    case VK_FORMAT_R32G32B32A32_SINT: return wgpu::VertexFormat::Sint32x4;
    case VK_FORMAT_R8G8B8A8_UNORM: return wgpu::VertexFormat::Unorm8x4;
    case VK_FORMAT_R8G8B8A8_SNORM: return wgpu::VertexFormat::Snorm8x4;
    case VK_FORMAT_R8G8B8A8_UINT: return wgpu::VertexFormat::Uint8x4;
    case VK_FORMAT_R8G8B8A8_SINT: return wgpu::VertexFormat::Sint8x4;
    case VK_FORMAT_R8G8_UNORM: return wgpu::VertexFormat::Unorm8x2;
    case VK_FORMAT_R16G16_SFLOAT: return wgpu::VertexFormat::Float16x2;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return wgpu::VertexFormat::Float16x4;
    case VK_FORMAT_R16G16_UNORM: return wgpu::VertexFormat::Unorm16x2;
    case VK_FORMAT_R16G16B16A16_UNORM: return wgpu::VertexFormat::Unorm16x4;
    default: break;
    }
    throw std::runtime_error("WebGPU: VkFormat " + std::to_string(static_cast<int>(format)) + " has no vertex format");
}

wgpu::BlendFactor BlendFactor(VkBlendFactor factor) {
    switch (factor) {
    case VK_BLEND_FACTOR_ZERO: return wgpu::BlendFactor::Zero;
    case VK_BLEND_FACTOR_ONE: return wgpu::BlendFactor::One;
    case VK_BLEND_FACTOR_SRC_COLOR: return wgpu::BlendFactor::Src;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return wgpu::BlendFactor::OneMinusSrc;
    case VK_BLEND_FACTOR_DST_COLOR: return wgpu::BlendFactor::Dst;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return wgpu::BlendFactor::OneMinusDst;
    case VK_BLEND_FACTOR_SRC_ALPHA: return wgpu::BlendFactor::SrcAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return wgpu::BlendFactor::OneMinusSrcAlpha;
    case VK_BLEND_FACTOR_DST_ALPHA: return wgpu::BlendFactor::DstAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return wgpu::BlendFactor::OneMinusDstAlpha;
    case VK_BLEND_FACTOR_CONSTANT_COLOR:
    case VK_BLEND_FACTOR_CONSTANT_ALPHA: return wgpu::BlendFactor::Constant;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return wgpu::BlendFactor::OneMinusConstant;
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return wgpu::BlendFactor::SrcAlphaSaturated;
    default: break;
    }
    throw std::runtime_error("WebGPU: blend factor " + std::to_string(static_cast<int>(factor)) + " is not supported");
}

wgpu::BlendOperation BlendOperation(VkBlendOp operation) {
    switch (operation) {
    case VK_BLEND_OP_ADD: return wgpu::BlendOperation::Add;
    case VK_BLEND_OP_SUBTRACT: return wgpu::BlendOperation::Subtract;
    case VK_BLEND_OP_REVERSE_SUBTRACT: return wgpu::BlendOperation::ReverseSubtract;
    case VK_BLEND_OP_MIN: return wgpu::BlendOperation::Min;
    case VK_BLEND_OP_MAX: return wgpu::BlendOperation::Max;
    default: break;
    }
    throw std::runtime_error("WebGPU: blend operation " + std::to_string(static_cast<int>(operation)) + " is not supported");
}

wgpu::AddressMode AddressMode(VkSamplerAddressMode mode) {
    switch (mode) {
    case VK_SAMPLER_ADDRESS_MODE_REPEAT: return wgpu::AddressMode::Repeat;
    case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return wgpu::AddressMode::MirrorRepeat;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
    case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return wgpu::AddressMode::ClampToEdge;
    default: break;
    }
    throw std::runtime_error("WebGPU: sampler address mode " + std::to_string(static_cast<int>(mode)) + " is not supported");
}

std::set<std::uint32_t> DeclaredBindings(const std::string& source) {
    std::set<std::uint32_t> result;
    for (auto found = source.find("@binding("); found != std::string::npos; found = source.find("@binding(", found + 1)) result.insert(static_cast<std::uint32_t>(std::stoul(source.substr(found + 9))));
    return result;
}

std::string MatchVertexOutputs(std::string vertex, const std::string& pixel) {
    for (auto found = pixel.find("@location("); found != std::string::npos; found = pixel.find("@location(", found + 1)) {
        const auto location = static_cast<std::uint32_t>(std::stoul(pixel.substr(found + 10)));
        const auto end = pixel.find('\n', found);
        const auto declaration = pixel.substr(found, end == std::string::npos ? std::string::npos : end - found);
        if (declaration.find("aps5_parameter") == std::string::npos) continue;
        vertex = ShaderRecompiler::SpecializeWgslVertexOutput(std::move(vertex), location, declaration.find("@interpolate(flat)") != std::string::npos, declaration.find("@interpolate(linear") != std::string::npos);
    }
    return vertex;
}

enum class ResourceKind { Buffer, Texture, Sampler };

struct TextureSource {
    std::uint64_t address = 0;
    bool reuse = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm;
    std::uint32_t bytesPerRow = 0;
    std::vector<std::byte> texels;
};

struct SamplerKey {
    wgpu::FilterMode mag = wgpu::FilterMode::Nearest;
    wgpu::FilterMode min = wgpu::FilterMode::Nearest;
    wgpu::MipmapFilterMode mip = wgpu::MipmapFilterMode::Nearest;
    wgpu::AddressMode u = wgpu::AddressMode::ClampToEdge;
    wgpu::AddressMode v = wgpu::AddressMode::ClampToEdge;
    wgpu::AddressMode w = wgpu::AddressMode::ClampToEdge;
    float minLod = 0.0f;
    float maxLod = 32.0f;

    auto Tie() const { return std::tie(mag, min, mip, u, v, w, minLod, maxLod); }
    bool operator<(const SamplerKey& other) const { return Tie() < other.Tie(); }
};

struct Resource {
    std::uint32_t binding = 0;
    ResourceKind kind = ResourceKind::Buffer;
    std::vector<std::byte> bytes;
    TextureSource texture;
    SamplerKey sampler;
};

struct StageJob {
    std::string source;
    std::vector<Resource> resources;
};

struct VertexStream {
    std::uint32_t location;
    wgpu::VertexFormat format;
    std::uint32_t stride;
    std::vector<std::byte> bytes;
};

struct TargetInfo {
    std::uint64_t address = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm;
};

struct DrawJob {
    StageJob vertex;
    std::optional<StageJob> pixel;
    std::vector<VertexStream> streams;
    std::vector<std::byte> indices;
    wgpu::IndexFormat indexFormat = wgpu::IndexFormat::Undefined;
    std::uint32_t count = 0;
    std::uint32_t instances = 1;
    std::uint32_t firstVertex = 0;
    bool rect = false;
    wgpu::PrimitiveTopology topology = wgpu::PrimitiveTopology::TriangleList;
    wgpu::CullMode cull = wgpu::CullMode::None;
    wgpu::FrontFace front = wgpu::FrontFace::CCW;
    TargetInfo target;
    bool blend = false;
    wgpu::BlendComponent color;
    wgpu::BlendComponent alpha;
    wgpu::ColorWriteMask writeMask = wgpu::ColorWriteMask::All;
    std::array<double, 4> constants{};
    std::array<float, 6> viewport{};
    std::array<std::uint32_t, 4> scissor{};
    bool flipY = false;
    bool depthHalf = false;
};

struct PresentJob {
    AgcDriver::DisplayBuffer buffer;
    std::vector<std::byte> pixels;
};

constexpr const char* PresentShader = R"(@group(0) @binding(0) var aps5_source: texture_2d<f32>;
@group(0) @binding(1) var aps5_sampler: sampler;

struct Aps5Varying {
    @builtin(position) position: vec4<f32>,
    @location(0) coordinate: vec2<f32>,
}

@vertex
fn vertexMain(@builtin(vertex_index) index: u32) -> Aps5Varying {
    let coordinate = vec2<f32>(f32((index << 1u) & 2u), f32(index & 2u));
    var result: Aps5Varying;
    result.position = vec4<f32>(coordinate.x * 2.0 - 1.0, 1.0 - coordinate.y * 2.0, 0.0, 1.0);
    result.coordinate = coordinate;
    return result;
}

@fragment
fn fragmentMain(input: Aps5Varying) -> @location(0) vec4<f32> {
    return vec4<f32>(textureSample(aps5_source, aps5_sampler, input.coordinate).rgb, 1.0);
}
)";

class Device {
public:
    static Device& Get() {
        static Device device;
        return device;
    }

    void Post(std::function<void()> task) {
        auto* heap = new std::function<void()>(std::move(task));
        if (!emscripten_proxy_async(queue, thread, [](void* argument) {
                std::unique_ptr<std::function<void()>> task(static_cast<std::function<void()>*>(argument));
                try {
                    (*task)();
                } catch (const std::exception& error) {
                    WasmRuntime::GuestFault("WebGPU thread failed: %s", error.what());
                }
            }, heap)) {
            delete heap;
            throw std::runtime_error("WebGPU: cannot post to the GPU thread");
        }
    }

    void NoteTarget(std::uint64_t address) {
        std::lock_guard lock(mutex);
        targetAddresses.insert(address);
    }

    bool IsTarget(std::uint64_t address) {
        std::lock_guard lock(mutex);
        return targetAddresses.contains(address);
    }

    bool Unchanged(const TextureSource& source, std::uint64_t hash) {
        std::lock_guard lock(mutex);
        auto& known = uploaded[source.address];
        const auto key = std::make_tuple(hash, source.width, source.height, source.format, source.bytesPerRow);
        if (known == key) return true;
        known = key;
        return false;
    }

    void Draw(std::shared_ptr<DrawJob> job) {
        Post([this, job] { executeDraw(*job); });
    }

    void Present(std::shared_ptr<PresentJob> job) {
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return presentsDone + 1 >= presentsPosted; });
            ++presentsPosted;
        }
        Post([this, job] {
            executePresent(*job);
            gpuQueue.OnSubmittedWorkDone(wgpu::CallbackMode::AllowSpontaneous, [this](wgpu::QueueWorkDoneStatus, wgpu::StringView) {
                std::lock_guard lock(mutex);
                ++presentsDone;
                changed.notify_all();
            });
        });
    }

    void AfterWork(std::function<void()> completion) {
        Post(std::move(completion));
    }

private:
    struct RenderTarget {
        wgpu::Texture texture;
        wgpu::TextureView view;
        TargetInfo info;
        bool initialized = false;
    };

    struct Upload {
        wgpu::Texture texture;
        wgpu::TextureView view;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    };

    Device() {
        queue = em_proxying_queue_create();
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        emscripten_pthread_attr_settransferredcanvases(&attributes, "#canvas");
        Require(pthread_create(&thread, &attributes, &Device::Run, this) == 0, "cannot create the GPU thread");
        pthread_attr_destroy(&attributes);
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return state != 0; });
        Require(state > 0, "the GPU device is not available: " + failure);
    }

    static void* Run(void* argument) {
        static_cast<Device*>(argument)->initialize();
        emscripten_runtime_keepalive_push();
        return nullptr;
    }

    void finishInitialization(int result, std::string reason = {}) {
        std::lock_guard lock(mutex);
        state = result;
        failure = std::move(reason);
        changed.notify_all();
    }

    void initialize() {
        instance = wgpu::CreateInstance(nullptr);
        wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas;
        canvas.selector = "#canvas";
        wgpu::SurfaceDescriptor surfaceDescriptor;
        surfaceDescriptor.nextInChain = &canvas;
        surface = instance.CreateSurface(&surfaceDescriptor);
        wgpu::RequestAdapterOptions options;
        options.compatibleSurface = surface;
        options.powerPreference = wgpu::PowerPreference::HighPerformance;
        instance.RequestAdapter(&options, wgpu::CallbackMode::AllowSpontaneous, [this](wgpu::RequestAdapterStatus status, wgpu::Adapter result, wgpu::StringView message) {
            if (status != wgpu::RequestAdapterStatus::Success) return finishInitialization(-1, std::string(message.data, message.length));
            adapter = std::move(result);
            wgpu::DeviceDescriptor descriptor;
            descriptor.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
                static std::atomic<int> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[webgpu] error %d: %.*s\n", static_cast<int>(type), static_cast<int>(message.length), message.data);
            });
            adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous, [this](wgpu::RequestDeviceStatus status, wgpu::Device result, wgpu::StringView message) {
                if (status != wgpu::RequestDeviceStatus::Success) return finishInitialization(-1, std::string(message.data, message.length));
                device = std::move(result);
                gpuQueue = device.GetQueue();
                wgpu::SurfaceCapabilities capabilities;
                surface.GetCapabilities(adapter, &capabilities);
                surfaceFormat = capabilities.formatCount != 0 ? capabilities.formats[0] : wgpu::TextureFormat::BGRA8Unorm;
                wgpu::SurfaceConfiguration configuration;
                configuration.device = device;
                configuration.format = surfaceFormat;
                configuration.usage = wgpu::TextureUsage::RenderAttachment;
                configuration.width = CanvasWidth;
                configuration.height = CanvasHeight;
                configuration.alphaMode = wgpu::CompositeAlphaMode::Opaque;
                configuration.presentMode = wgpu::PresentMode::Fifo;
                surface.Configure(&configuration);
                std::fprintf(stderr, "[webgpu] device ready, canvas format %d\n", static_cast<int>(surfaceFormat));
                finishInitialization(1);
            });
        });
    }

    wgpu::ShaderModule module(const std::string& source) {
        if (const auto found = modules.find(source); found != modules.end()) return found->second;
        wgpu::ShaderSourceWGSL wgsl;
        wgsl.code = source.c_str();
        wgpu::ShaderModuleDescriptor descriptor;
        descriptor.nextInChain = &wgsl;
        auto result = device.CreateShaderModule(&descriptor);
        modules.emplace(source, result);
        return result;
    }

    RenderTarget& renderTarget(const TargetInfo& info) {
        auto& target = targets[info.address];
        if (target.texture && target.info.width == info.width && target.info.height == info.height && target.info.format == info.format) return target;
        wgpu::TextureDescriptor descriptor;
        descriptor.size = {info.width, info.height, 1};
        descriptor.format = info.format;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;
        target.texture = device.CreateTexture(&descriptor);
        target.view = target.texture.CreateView();
        target.info = info;
        target.initialized = false;
        return target;
    }

    wgpu::TextureView uploadTexture(const TextureSource& source) {
        if (source.reuse) {
            const auto found = uploads.find(source.address);
            Require(found != uploads.end() && found->second.width == source.width && found->second.height == source.height && found->second.format == source.format, "a reused texture upload is missing");
            return found->second.view;
        }
        if (source.texels.empty()) {
            if (const auto found = targets.find(source.address); found != targets.end()) return found->second.view;
            return renderTarget({source.address, source.width, source.height, source.format}).view;
        }
        auto& upload = uploads[source.address];
        if (!upload.texture || upload.width != source.width || upload.height != source.height || upload.format != source.format) {
            wgpu::TextureDescriptor descriptor;
            descriptor.size = {source.width, source.height, 1};
            descriptor.format = source.format;
            descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
            upload.texture = device.CreateTexture(&descriptor);
            upload.view = upload.texture.CreateView();
            upload.width = source.width;
            upload.height = source.height;
            upload.format = source.format;
        }
        wgpu::TexelCopyTextureInfo destination;
        destination.texture = upload.texture;
        wgpu::TexelCopyBufferLayout layout;
        layout.bytesPerRow = source.bytesPerRow;
        layout.rowsPerImage = source.height;
        const wgpu::Extent3D extent{source.width, source.height, 1};
        gpuQueue.WriteTexture(&destination, source.texels.data(), source.texels.size(), &layout, &extent);
        return upload.view;
    }

    wgpu::Sampler sampler(const SamplerKey& key) {
        if (const auto found = samplers.find(key); found != samplers.end()) return found->second;
        wgpu::SamplerDescriptor descriptor;
        descriptor.addressModeU = key.u;
        descriptor.addressModeV = key.v;
        descriptor.addressModeW = key.w;
        descriptor.magFilter = key.mag;
        descriptor.minFilter = key.min;
        descriptor.mipmapFilter = key.mip;
        descriptor.lodMinClamp = key.minLod;
        descriptor.lodMaxClamp = std::max(key.minLod, key.maxLod);
        auto result = device.CreateSampler(&descriptor);
        samplers.emplace(key, result);
        return result;
    }

    wgpu::Buffer buffer(const std::vector<std::byte>& bytes, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = std::max<std::uint64_t>((bytes.size() + 3u) & ~std::size_t{3}, 16u);
        descriptor.usage = usage | wgpu::BufferUsage::CopyDst;
        auto result = device.CreateBuffer(&descriptor);
        if (!bytes.empty()) gpuQueue.WriteBuffer(result, 0, bytes.data(), bytes.size());
        return result;
    }

    std::string pipelineKey(const DrawJob& job) const {
        std::string key = job.vertex.source;
        key += '\x01';
        if (job.pixel) key += job.pixel->source;
        key += '\x01';
        for (const auto& stream : job.streams) key += std::to_string(stream.location) + ":" + std::to_string(static_cast<int>(stream.format)) + ":" + std::to_string(stream.stride) + ";";
        key += std::to_string(static_cast<int>(job.target.format)) + "/" + std::to_string(static_cast<int>(job.topology)) + "/" + std::to_string(static_cast<int>(job.indexFormat)) + "/" + std::to_string(job.rect) + "/" + std::to_string(static_cast<int>(job.cull)) + "/" + std::to_string(static_cast<int>(job.front)) + "/" + std::to_string(job.flipY) + "/" + std::to_string(job.depthHalf) + "/" + std::to_string(static_cast<std::uint64_t>(job.writeMask));
        if (job.blend) {
            for (const auto& component : {job.color, job.alpha}) key += "/" + std::to_string(static_cast<int>(component.operation)) + "," + std::to_string(static_cast<int>(component.srcFactor)) + "," + std::to_string(static_cast<int>(component.dstFactor));
        }
        return key;
    }

    wgpu::RenderPipeline pipeline(const DrawJob& job) {
        const auto key = pipelineKey(job);
        if (const auto found = pipelines.find(key); found != pipelines.end()) return found->second;
        std::vector<wgpu::VertexAttribute> attributes(job.streams.size());
        std::vector<wgpu::VertexBufferLayout> layouts(job.streams.size());
        for (std::size_t index = 0; index < job.streams.size(); ++index) {
            attributes[index].format = job.streams[index].format;
            attributes[index].offset = 0;
            attributes[index].shaderLocation = job.streams[index].location;
            layouts[index].arrayStride = job.streams[index].stride;
            layouts[index].stepMode = wgpu::VertexStepMode::Vertex;
            layouts[index].attributeCount = 1;
            layouts[index].attributes = &attributes[index];
        }
        const std::array<wgpu::ConstantEntry, 2> constants{wgpu::ConstantEntry{nullptr, "aps5_flip_y", job.flipY ? 1.0 : 0.0}, wgpu::ConstantEntry{nullptr, "aps5_depth_half", job.depthHalf ? 1.0 : 0.0}};
        wgpu::RenderPipelineDescriptor descriptor;
        descriptor.vertex.module = module(job.vertex.source);
        descriptor.vertex.entryPoint = job.rect ? "aps5_rect" : "main";
        descriptor.vertex.constantCount = constants.size();
        descriptor.vertex.constants = constants.data();
        descriptor.vertex.bufferCount = layouts.size();
        descriptor.vertex.buffers = layouts.data();
        descriptor.primitive.topology = job.rect ? wgpu::PrimitiveTopology::TriangleList : job.topology;
        if (job.topology == wgpu::PrimitiveTopology::TriangleStrip || job.topology == wgpu::PrimitiveTopology::LineStrip) descriptor.primitive.stripIndexFormat = job.indexFormat;
        descriptor.primitive.cullMode = job.cull;
        descriptor.primitive.frontFace = job.front;
        wgpu::BlendState blend{job.color, job.alpha};
        wgpu::ColorTargetState target;
        target.format = job.target.format;
        target.blend = job.blend ? &blend : nullptr;
        target.writeMask = job.writeMask;
        wgpu::FragmentState fragment;
        if (job.pixel) {
            fragment.module = module(job.pixel->source);
            fragment.entryPoint = "main";
            fragment.targetCount = 1;
            fragment.targets = &target;
            descriptor.fragment = &fragment;
        }
        auto result = device.CreateRenderPipeline(&descriptor);
        pipelines.emplace(key, result);
        if (pipelines.size() <= 16) std::fprintf(stderr, "[webgpu] pipeline %zu: target %ux%u, %zu streams, %s\n", pipelines.size(), job.target.width, job.target.height, job.streams.size(), job.rect ? "rect list" : "triangles");
        return result;
    }

    wgpu::BindGroup bindGroup(const wgpu::RenderPipeline& renderPipeline, std::uint32_t group, const StageJob* stage) {
        std::vector<wgpu::BindGroupEntry> entries;
        if (stage != nullptr) {
            for (const auto& resource : stage->resources) {
                wgpu::BindGroupEntry entry;
                entry.binding = resource.binding;
                switch (resource.kind) {
                case ResourceKind::Buffer: {
                    entry.buffer = buffer(resource.bytes, wgpu::BufferUsage::Storage);
                    break;
                }
                case ResourceKind::Texture:
                    entry.textureView = uploadTexture(resource.texture);
                    break;
                case ResourceKind::Sampler:
                    entry.sampler = sampler(resource.sampler);
                    break;
                }
                entries.push_back(entry);
            }
        }
        wgpu::BindGroupDescriptor descriptor;
        descriptor.layout = renderPipeline.GetBindGroupLayout(group);
        descriptor.entryCount = entries.size();
        descriptor.entries = entries.data();
        return device.CreateBindGroup(&descriptor);
    }

    void executeDraw(const DrawJob& job) {
        auto renderPipeline = pipeline(job);
        const bool pixelResources = job.pixel && !job.pixel->resources.empty();
        std::vector<wgpu::BindGroup> groups;
        if (!job.vertex.resources.empty() || pixelResources) groups.push_back(bindGroup(renderPipeline, 0, &job.vertex));
        if (pixelResources) groups.push_back(bindGroup(renderPipeline, 1, &*job.pixel));
        std::vector<wgpu::Buffer> streams;
        for (const auto& stream : job.streams) streams.push_back(buffer(stream.bytes, wgpu::BufferUsage::Vertex));
        wgpu::Buffer indices;
        if (job.indexFormat != wgpu::IndexFormat::Undefined) indices = buffer(job.indices, wgpu::BufferUsage::Index);
        auto& target = renderTarget(job.target);
        wgpu::RenderPassColorAttachment attachment;
        attachment.view = target.view;
        attachment.loadOp = target.initialized ? wgpu::LoadOp::Load : wgpu::LoadOp::Clear;
        attachment.storeOp = wgpu::StoreOp::Store;
        attachment.clearValue = {0.0, 0.0, 0.0, 0.0};
        wgpu::RenderPassDescriptor passDescriptor;
        passDescriptor.colorAttachmentCount = 1;
        passDescriptor.colorAttachments = &attachment;
        auto encoder = device.CreateCommandEncoder();
        auto pass = encoder.BeginRenderPass(&passDescriptor);
        pass.SetPipeline(renderPipeline);
        for (std::uint32_t index = 0; index < groups.size(); ++index) pass.SetBindGroup(index, groups[index]);
        for (std::uint32_t index = 0; index < streams.size(); ++index) pass.SetVertexBuffer(index, streams[index]);
        pass.SetViewport(job.viewport[0], job.viewport[1], job.viewport[2], job.viewport[3], job.viewport[4], job.viewport[5]);
        pass.SetScissorRect(job.scissor[0], job.scissor[1], job.scissor[2], job.scissor[3]);
        const wgpu::Color constants{job.constants[0], job.constants[1], job.constants[2], job.constants[3]};
        pass.SetBlendConstant(&constants);
        if (job.rect) {
            pass.Draw(job.count / 3u * 6u, job.instances, 0, 0);
        } else if (indices) {
            pass.SetIndexBuffer(indices, job.indexFormat);
            pass.DrawIndexed(job.count, job.instances, 0, 0, 0);
        } else {
            pass.Draw(job.count, job.instances, job.firstVertex, 0);
        }
        pass.End();
        auto commands = encoder.Finish();
        gpuQueue.Submit(1, &commands);
        target.initialized = true;
        ++draws;
    }

    void executePresent(const PresentJob& job) {
        wgpu::TextureView source;
        if (const auto found = targets.find(job.buffer.address); found != targets.end() && found->second.initialized) {
            source = found->second.view;
        } else {
            TextureSource texture{job.buffer.address, false, job.buffer.width, job.buffer.height, wgpu::TextureFormat::BGRA8Unorm, job.buffer.width * 4u, job.pixels};
            source = uploadTexture(texture);
        }
        if (!presentPipeline) {
            wgpu::ColorTargetState target;
            target.format = surfaceFormat;
            wgpu::FragmentState fragment;
            fragment.module = module(PresentShader);
            fragment.entryPoint = "fragmentMain";
            fragment.targetCount = 1;
            fragment.targets = &target;
            wgpu::RenderPipelineDescriptor descriptor;
            descriptor.vertex.module = module(PresentShader);
            descriptor.vertex.entryPoint = "vertexMain";
            descriptor.fragment = &fragment;
            presentPipeline = device.CreateRenderPipeline(&descriptor);
            presentSampler = sampler({wgpu::FilterMode::Linear, wgpu::FilterMode::Linear, wgpu::MipmapFilterMode::Nearest, wgpu::AddressMode::ClampToEdge, wgpu::AddressMode::ClampToEdge, wgpu::AddressMode::ClampToEdge, 0.0f, 0.0f});
        }
        std::array<wgpu::BindGroupEntry, 2> entries;
        entries[0].binding = 0;
        entries[0].textureView = source;
        entries[1].binding = 1;
        entries[1].sampler = presentSampler;
        wgpu::BindGroupDescriptor groupDescriptor;
        groupDescriptor.layout = presentPipeline.GetBindGroupLayout(0);
        groupDescriptor.entryCount = entries.size();
        groupDescriptor.entries = entries.data();
        auto group = device.CreateBindGroup(&groupDescriptor);
        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);
        Require(surfaceTexture.texture != nullptr, "the canvas has no current texture");
        wgpu::RenderPassColorAttachment attachment;
        attachment.view = surfaceTexture.texture.CreateView();
        attachment.loadOp = wgpu::LoadOp::Clear;
        attachment.storeOp = wgpu::StoreOp::Store;
        attachment.clearValue = {0.0, 0.0, 0.0, 1.0};
        wgpu::RenderPassDescriptor passDescriptor;
        passDescriptor.colorAttachmentCount = 1;
        passDescriptor.colorAttachments = &attachment;
        auto encoder = device.CreateCommandEncoder();
        auto pass = encoder.BeginRenderPass(&passDescriptor);
        pass.SetPipeline(presentPipeline);
        pass.SetBindGroup(0, group);
        pass.Draw(3);
        pass.End();
        auto commands = encoder.Finish();
        gpuQueue.Submit(1, &commands);
        ++presents;
        if ((presents & (presents - 1)) == 0) std::fprintf(stderr, "[webgpu] present %llu: buffer 0x%llx %ux%u, %llu draws\n", static_cast<unsigned long long>(presents), static_cast<unsigned long long>(job.buffer.address), job.buffer.width, job.buffer.height, static_cast<unsigned long long>(draws));
    }

    pthread_t thread{};
    em_proxying_queue* queue = nullptr;
    std::mutex mutex;
    std::condition_variable changed;
    int state = 0;
    std::string failure;
    std::set<std::uint64_t> targetAddresses;
    std::map<std::uint64_t, std::tuple<std::uint64_t, std::uint32_t, std::uint32_t, wgpu::TextureFormat, std::uint32_t>> uploaded;
    std::uint64_t presentsPosted = 0;
    std::uint64_t presentsDone = 0;

    wgpu::Instance instance;
    wgpu::Surface surface;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue gpuQueue;
    wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::BGRA8Unorm;
    std::unordered_map<std::string, wgpu::ShaderModule> modules;
    std::unordered_map<std::string, wgpu::RenderPipeline> pipelines;
    std::map<std::uint64_t, RenderTarget> targets;
    std::map<std::uint64_t, Upload> uploads;
    std::map<SamplerKey, wgpu::Sampler> samplers;
    wgpu::RenderPipeline presentPipeline;
    wgpu::Sampler presentSampler;
    std::uint64_t draws = 0;
    std::uint64_t presents = 0;
};

std::uint32_t MemoryOffset(const ShaderRecompiler::RecompileResult& result, std::uint32_t element) {
    const auto data = std::ranges::find_if(result.bindings, [](const auto& binding) { return binding.role == DescriptorRole::ShaderData; });
    if (data == result.bindings.end()) return 0;
    const auto index = result.memoryOffsetDword + element / 4u;
    Require(index < data->guestDescriptor.size(), "the shader data has no memory offset for buffer element " + std::to_string(element));
    return (data->guestDescriptor[index] >> ((element % 4u) * 8u)) & 0xffu;
}

TextureSource DescribeTexture(std::span<const std::uint32_t> words, Device& gpu) {
    namespace Graphics = AgcDriver::Graphics;
    Require(!Graphics::IsNullTextureResource(words), "null textures are not implemented");
    const auto texture = Graphics::DecodeTextureResource(words);
    Require(texture.dimension == Graphics::TextureDimension::k2D, "only 2D textures are implemented");
    TextureSource source;
    source.address = texture.baseAddress;
    source.width = texture.width;
    source.height = texture.height;
    source.format = TextureFormat(Graphics::ResolveTextureFormat(texture.format));
    if (gpu.IsTarget(texture.baseAddress)) return source;
    Require(texture.tileMode == Graphics::TextureTileMode::kLinear, "tiled texture uploads are not implemented (tile mode " + std::to_string(static_cast<int>(texture.tileMode)) + ")");
    Require(!Graphics::IsBlockCompressed(texture.format), "block-compressed textures are not implemented");
    const auto layout = Graphics::ComputeMipLayout(texture.tileMode, texture.format, texture.width, texture.height, 1).front();
    source.bytesPerRow = layout.pitchBytes;
    const auto address = texture.baseAddress + layout.linearOffset;
    const auto bytes = static_cast<std::uint64_t>(layout.pitchBytes) * texture.height;
    source.reuse = gpu.Unchanged(source, Hash(address, bytes));
    if (!source.reuse) source.texels = ReadGuest(address, bytes);
    return source;
}

SamplerKey DescribeSampler(std::span<const std::uint32_t> words) {
    const auto sampler = AgcDriver::Graphics::DecodeSamplerResource(words);
    Require(!sampler.unnormalizedCoordinates, "unnormalized samplers are not implemented");
    Require(!sampler.compareEnable, "comparison samplers are not implemented");
    SamplerKey key;
    key.mag = sampler.magFilter == VK_FILTER_LINEAR ? wgpu::FilterMode::Linear : wgpu::FilterMode::Nearest;
    key.min = sampler.minFilter == VK_FILTER_LINEAR ? wgpu::FilterMode::Linear : wgpu::FilterMode::Nearest;
    key.mip = sampler.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR ? wgpu::MipmapFilterMode::Linear : wgpu::MipmapFilterMode::Nearest;
    key.u = AddressMode(sampler.addressModeU);
    key.v = AddressMode(sampler.addressModeV);
    key.w = AddressMode(sampler.addressModeW);
    key.minLod = std::max(sampler.minLod, 0.0f);
    key.maxLod = std::max(sampler.maxLod, key.minLod);
    return key;
}

StageJob DescribeStage(const ShaderRecompiler::RecompileResult& result, std::string source, Device& gpu) {
    StageJob stage;
    const auto declared = DeclaredBindings(source);
    stage.source = std::move(source);
    std::uint32_t number = 0;
    for (const auto& binding : result.bindings) {
        for (std::uint32_t element = 0; element < binding.count; ++element, ++number) {
            if (!declared.contains(number)) continue;
            Resource resource;
            resource.binding = number;
            const auto words = std::span(binding.guestDescriptor);
            switch (binding.role) {
            case DescriptorRole::ShaderData:
                resource.bytes.resize(words.size() * sizeof(std::uint32_t));
                std::memcpy(resource.bytes.data(), words.data(), resource.bytes.size());
                break;
            case DescriptorRole::GuestBuffers: {
                Require(words.size() >= (element + 1u) * 4u, "a guest buffer element has no V#");
                const auto v = words.subspan(element * 4u, 4);
                const auto base = static_cast<std::uint64_t>(v[0]) | (static_cast<std::uint64_t>(v[1] & 0xffffu) << 32u);
                const auto stride = (v[1] >> 16u) & 0x3fffu;
                const std::uint64_t size = stride == 0 ? v[2] : static_cast<std::uint64_t>(stride) * v[2];
                const auto offset = MemoryOffset(result, element);
                resource.bytes = ReadGuest(base - offset, std::min<std::uint64_t>(size + offset, BufferLimit));
                break;
            }
            case DescriptorRole::GuestImages:
                Require(words.size() >= (element + 1u) * 8u, "a guest image element has no T#");
                resource.kind = ResourceKind::Texture;
                resource.texture = DescribeTexture(words.subspan(element * 8u, 8), gpu);
                break;
            case DescriptorRole::GuestSamplers:
                Require(words.size() >= (element + 1u) * 4u, "a guest sampler element has no S#");
                resource.kind = ResourceKind::Sampler;
                resource.sampler = DescribeSampler(words.subspan(element * 4u, 4));
                break;
            default:
                throw std::runtime_error("WebGPU: binding role " + std::to_string(static_cast<int>(binding.role)) + " is not implemented");
            }
            stage.resources.push_back(std::move(resource));
        }
    }
    return stage;
}

std::shared_ptr<DrawJob> BuildDrawJob(std::span<const std::uint32_t> packet, const AgcDriver::QueueState& queue, Device& gpu) {
    auto requests = BuildDrawRequests(DecodeDraw(packet, queue));
    const auto& draw = requests->stages;
    const auto& state = draw.state;
    Require(state.hasColorTarget && state.colors.size() == 1, "draws need exactly one color target");
    Require(!state.depth && !state.depthTest && !state.depthWrite, "depth targets are not implemented");
    auto& compiled = requests->requests;
    Require(compiled.size() > 1, "draws without a pixel program are not implemented");
    const auto vertex = ShaderRecompiler::Recompile(compiled.front());
    std::optional<ShaderRecompiler::RecompileResult> pixel;
    if (compiled.size() > 1) {
        auto& request = compiled[1];
        request.context.pixel = AgcDriver::Graphics::DefaultUnexportedInputs(*request.context.pixel, vertex.parameterExports);
        pixel = ShaderRecompiler::Recompile(request);
    }
    auto job = std::make_shared<DrawJob>();
    const auto& color = state.color;
    job->target = {color.address, color.extent.width, color.extent.height, TextureFormat(color.format)};
    gpu.NoteTarget(color.address);
    if (pixel) job->pixel = DescribeStage(*pixel, pixel->wgsl, gpu);
    job->vertex = DescribeStage(vertex, pixel ? MatchVertexOutputs(vertex.wgsl, pixel->wgsl) : vertex.wgsl, gpu);

    const auto& parameters = draw.draw;
    job->count = parameters.indexCount;
    job->instances = std::max(parameters.instanceCount, 1u);
    job->firstVertex = parameters.firstVertex;
    std::uint32_t maxIndex = parameters.firstVertex + std::max(parameters.indexCount, 1u) - 1u;
    if (parameters.indexed) {
        Require(parameters.indexSize == 2 || parameters.indexSize == 4, "unsupported index size");
        job->indexFormat = parameters.indexSize == 2 ? wgpu::IndexFormat::Uint16 : wgpu::IndexFormat::Uint32;
        job->indices = ReadGuest(parameters.indexAddress, static_cast<std::uint64_t>(parameters.indexCount) * parameters.indexSize);
        maxIndex = 0;
        for (std::uint32_t index = 0; index < parameters.indexCount; ++index) {
            std::uint32_t value = 0;
            std::memcpy(&value, job->indices.data() + index * parameters.indexSize, parameters.indexSize);
            maxIndex = std::max(maxIndex, value);
        }
    }
    for (const auto& attribute : vertex.vertexAttributes) {
        Require(attribute.fetchIndex == 0, "instanced vertex fetch is not implemented");
        Require(!AgcDriver::Graphics::NullVertexDescriptor(attribute), "null vertex descriptors are not implemented");
        const auto format = AgcDriver::Graphics::DecodeVertexFormat(attribute);
        const auto& fields = attribute.resource.fields;
        const auto base = static_cast<std::uint64_t>(fields[0]) | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        const auto stride = (fields[1] >> 16u) & 0x3fffu;
        Require(stride != 0 && stride % 4u == 0, "vertex stride " + std::to_string(stride) + " is not supported");
        job->streams.push_back({attribute.location, VertexFormat(format.format), stride, ReadGuest(base, static_cast<std::uint64_t>(stride) * maxIndex + format.bytes)});
    }

    switch (state.topology) {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: job->topology = wgpu::PrimitiveTopology::PointList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: job->topology = wgpu::PrimitiveTopology::LineList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: job->topology = wgpu::PrimitiveTopology::LineStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: job->topology = wgpu::PrimitiveTopology::TriangleList; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: job->topology = wgpu::PrimitiveTopology::TriangleStrip; break;
    default:
        Require(state.rectList, "primitive topology " + std::to_string(static_cast<int>(state.topology)) + " is not supported");
    }
    if (state.rectList) {
        Require(!parameters.indexed && job->streams.empty(), "indexed or vertex-buffer rect lists are not implemented");
        Require(job->vertex.source.find("fn aps5_rect(") != std::string::npos, "the vertex program has no rect list entry point");
        job->rect = true;
    }

    auto viewport = state.viewport;
    job->flipY = viewport.height > 0.0f;
    if (viewport.height < 0.0f) {
        viewport.y += viewport.height;
        viewport.height = -viewport.height;
    }
    const auto width = static_cast<float>(job->target.width);
    const auto height = static_cast<float>(job->target.height);
    const auto left = std::clamp(viewport.x, 0.0f, width);
    const auto top = std::clamp(viewport.y, 0.0f, height);
    job->viewport = {left, top, std::clamp(viewport.x + viewport.width, left, width) - left, std::clamp(viewport.y + viewport.height, top, height) - top, viewport.minDepth, viewport.maxDepth};
    job->depthHalf = state.negativeOneToOne;
    const auto scissorLeft = std::min(static_cast<std::uint32_t>(std::max(state.scissor.offset.x, 0)), job->target.width);
    const auto scissorTop = std::min(static_cast<std::uint32_t>(std::max(state.scissor.offset.y, 0)), job->target.height);
    job->scissor = {scissorLeft, scissorTop, std::min(state.scissor.extent.width, job->target.width - scissorLeft), std::min(state.scissor.extent.height, job->target.height - scissorTop)};

    job->cull = state.cullMode == VK_CULL_MODE_NONE ? wgpu::CullMode::None : state.cullMode == VK_CULL_MODE_FRONT_BIT ? wgpu::CullMode::Front : state.cullMode == VK_CULL_MODE_BACK_BIT ? wgpu::CullMode::Back : wgpu::CullMode::None;
    Require(state.cullMode != VK_CULL_MODE_FRONT_AND_BACK, "culling both faces is not implemented");
    const bool clockwise = state.frontFace == VK_FRONT_FACE_CLOCKWISE;
    job->front = clockwise != job->flipY ? wgpu::FrontFace::CW : wgpu::FrontFace::CCW;

    const auto& blend = state.blend;
    job->blend = blend.blendEnable != VK_FALSE;
    job->color = {BlendOperation(blend.colorBlendOp), BlendFactor(blend.srcColorBlendFactor), BlendFactor(blend.dstColorBlendFactor)};
    job->alpha = {BlendOperation(blend.alphaBlendOp), BlendFactor(blend.srcAlphaBlendFactor), BlendFactor(blend.dstAlphaBlendFactor)};
    job->writeMask = static_cast<wgpu::ColorWriteMask>(blend.colorWriteMask & 0xfu);
    job->constants = {state.blendConstants[0], state.blendConstants[1], state.blendConstants[2], state.blendConstants[3]};
    return job;
}

class WebGpuBackend final : public IBackend {
public:
    void RegisterShader(const Shader*) override {}

    void Draw(std::uint32_t, std::span<const std::uint32_t> packet, AgcDriver::QueueState& state) override {
        if (ShaderCaptureEnabled()) CaptureDraw(packet, state);
        static const bool null = std::getenv("APS5_WEBGPU_NULL") != nullptr;
        if (null) return;
        auto& gpu = Device::Get();
        gpu.Draw(BuildDrawJob(packet, state, gpu));
    }

    void Dispatch(std::uint32_t, std::span<const std::uint32_t>, AgcDriver::QueueState&) override {
        throw std::runtime_error("WebGPU: compute dispatches are not implemented");
    }

    void AfterWork(std::function<void()> completion) override {
        static const bool null = std::getenv("APS5_WEBGPU_NULL") != nullptr;
        if (null) return completion();
        Device::Get().AfterWork(std::move(completion));
    }

    void PresentBuffer(const AgcDriver::DisplayBuffer& buffer) override {
        static const bool null = std::getenv("APS5_WEBGPU_NULL") != nullptr;
        static std::atomic<std::uint64_t> count{0};
        if (null) {
            const auto presented = ++count;
            if ((presented & (presented - 1)) == 0) std::fprintf(stderr, "[webgpu] null present %llu\n", static_cast<unsigned long long>(presented));
            return;
        }
        auto& gpu = Device::Get();
        auto job = std::make_shared<PresentJob>();
        job->buffer = buffer;
        if (!gpu.IsTarget(buffer.address)) job->pixels = AgcDriver::ReadDisplayBuffer(buffer);
        gpu.Present(std::move(job));
    }

    void PresentClear(bool) override {}
};

}  // namespace

IBackend& Backend() {
    static WebGpuBackend backend;
    return backend;
}

}  // namespace WasmGpu
