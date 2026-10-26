#include "prx/libSceAgcDriver/Execution/include/ComputePipelineCapture.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace AgcDriver {
namespace {

class CaptureLimit final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class CaptureIoFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::atomic<bool> optionalDisabled{false};

void DisableOptionalCapture(const char* message) {
    if (!optionalDisabled.exchange(true, std::memory_order_relaxed)) std::fprintf(stderr, "[compute-capture] optional capture disabled: %s\n", message);
}

void RequireCapture(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("compute pipeline capture: ") + message);
}

void RequireCaptureIo(bool condition, const char* message) {
    if (!condition) throw CaptureIoFailure(std::string("compute pipeline capture: ") + message);
}

void RequireCaptureCapacity(bool condition, const char* message) {
    if (!condition) throw CaptureLimit(std::string("compute pipeline capture: ") + message);
}

void CheckCaptureIo(const std::error_code& error, const char* message) {
    if (error) throw CaptureIoFailure(std::string("compute pipeline capture: ") + message + ": " + error.message());
}

std::filesystem::file_status CaptureStatus(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error != std::errc::no_such_file_or_directory) CheckCaptureIo(error, "cannot inspect capture path");
    return status;
}

std::uintmax_t CaptureFileSize(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    CheckCaptureIo(error, "cannot size capture");
    return size;
}

std::string Hash(std::span<const std::byte> bytes) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto value : bytes) {
        hash ^= std::to_integer<std::uint8_t>(value);
        hash *= 1099511628211ull;
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0') << std::setw(16) << hash;
    return result.str();
}

class Encoder {
public:
    void Word(std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
    }
    void String(const std::string& value) {
        Word(static_cast<std::uint32_t>(value.size()));
        for (const auto character : value) bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    std::vector<std::byte> bytes;
};

class Decoder {
public:
    explicit Decoder(std::span<const std::byte> bytes) : bytes(bytes) {}
    std::uint32_t Word() {
        RequireCapture(bytes.size() - cursor >= 4, "truncated integer");
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) value |= std::to_integer<std::uint32_t>(bytes[cursor++]) << shift;
        return value;
    }
    std::uint32_t Count(std::uint32_t maximum, std::size_t minimumBytes) {
        const auto count = Word();
        RequireCapture(count <= maximum && count <= (bytes.size() - cursor) / minimumBytes, "count exceeds record bounds");
        return count;
    }
    std::string String() {
        const auto count = Count(VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1, 1);
        std::string result;
        for (std::uint32_t index = 0; index < count; ++index) result.push_back(static_cast<char>(std::to_integer<unsigned char>(bytes[cursor++])));
        return result;
    }
    bool Finished() const { return cursor == bytes.size(); }
private:
    std::span<const std::byte> bytes;
    std::size_t cursor = 0;
};

std::vector<std::byte> ReadBytes(const std::filesystem::path& path) {
    const auto size = CaptureFileSize(path);
    RequireCapture(size <= ComputePipelineCapture::MaxFileBytes, "file exceeds 36 MiB");
    std::ifstream input(path, std::ios::binary);
    RequireCaptureIo(input.good(), "cannot open capture");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    RequireCaptureIo(input.good() && input.peek() == std::char_traits<char>::eof(), "capture changed while reading");
    return bytes;
}

}

void ComputePipelineCapture::Validate() const {
    RequireCapture(spirv.size() >= 5 && spirv.size() <= MaxSpirvWords, "SPIR-V word count out of bounds");
    RequireCapture(spirv[0] == 0x07230203u && spirv[1] >= 0x00010000u && spirv[1] <= 0x00010600u && (spirv[1] & 0xffu) == 0 && spirv[3] != 0 && spirv[3] <= 0x003fffffu && spirv[4] == 0, "invalid SPIR-V header");
    bool mainEntry = false;
    for (std::size_t cursor = 5; cursor < spirv.size();) {
        const auto count = spirv[cursor] >> 16u;
        RequireCapture(count != 0 && count <= spirv.size() - cursor, "truncated SPIR-V instruction");
        if ((spirv[cursor] & 0xffffu) == 15u) {
            RequireCapture(count >= 5 && spirv[cursor + 1] == 5u && spirv[cursor + 2] != 0 && spirv[cursor + 2] < spirv[3] && spirv[cursor + 3] == 0x6e69616du && spirv[cursor + 4] == 0, "only a compute main entry point is supported");
            RequireCapture(!mainEntry, "multiple entry points are unsupported");
            mainEntry = true;
        }
        cursor += count;
    }
    RequireCapture(mainEntry, "compute main entry point missing");
    RequireCapture(deviceName.size() < VK_MAX_PHYSICAL_DEVICE_NAME_SIZE && deviceName.find('\0') == std::string::npos, "invalid device name");
    RequireCapture((flags & ~(VK_PIPELINE_CREATE_DISPATCH_BASE_BIT | VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR)) == 0, "unsupported pipeline flags");
    RequireCapture(stageFlags == 0 || (stageFlags == VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT && requiredSubgroupSize != 0), "unsupported shader stage flags");
    RequireCapture(requiredSubgroupSize == 0 || (requiredSubgroupSize <= 128 && (requiredSubgroupSize & (requiredSubgroupSize - 1u)) == 0), "invalid required subgroup size");
    RequireCapture(sets.size() <= 8, "too many descriptor sets");
    std::size_t totalBindings = 0;
    for (const auto& layout : sets) {
        RequireCapture((layout.flags & ~VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) == 0, "unsupported descriptor layout flags");
        RequireCapture(layout.bindings.size() <= 4096, "too many descriptor bindings");
        totalBindings += layout.bindings.size();
        std::set<std::uint32_t> used;
        const auto highest = layout.bindings.empty() ? 0u : std::ranges::max(layout.bindings, {}, &ComputeDescriptorBinding::binding).binding;
        for (const auto& binding : layout.bindings) {
            RequireCapture(used.insert(binding.binding).second, "duplicate descriptor binding");
            RequireCapture(binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || binding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || binding.type == VK_DESCRIPTOR_TYPE_SAMPLER, "unsupported descriptor type");
            RequireCapture(binding.count != 0 && binding.count <= 1048576, "invalid descriptor count");
            RequireCapture(binding.stages == VK_SHADER_STAGE_COMPUTE_BIT, "unsupported descriptor stage flags");
            constexpr VkDescriptorBindingFlags supported = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            RequireCapture((binding.flags & ~supported) == 0, "unsupported descriptor binding flags");
            RequireCapture((binding.flags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) == 0 || ((binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || binding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || binding.type == VK_DESCRIPTOR_TYPE_SAMPLER) && (layout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) != 0), "unsupported update-after-bind descriptor");
            RequireCapture((binding.flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) == 0 || binding.binding == highest, "variable descriptor count must use the highest binding");
        }
    }
    RequireCapture(totalBindings <= 4096, "too many total descriptor bindings");
    RequireCapture(pushConstants.size() <= 1, "too many compute push constant ranges");
    for (const auto& range : pushConstants) RequireCapture(range.stageFlags == VK_SHADER_STAGE_COMPUTE_BIT && range.size != 0 && range.size <= 4096 && range.offset <= 4096 - range.size && (range.offset % 4) == 0 && (range.size % 4) == 0, "invalid push constant range");
    RequireCapture(specialization.size() <= 4096, "too many specialization values");
    std::set<std::uint32_t> specializationIds;
    for (const auto& value : specialization) RequireCapture(specializationIds.insert(value.id).second, "duplicate specialization ID");
}

std::vector<std::byte> ComputePipelineCapture::Encode() const {
    Validate();
    Encoder output;
    output.Word(0x35535041u);
    output.Word(0x504d4f43u);
    output.Word(Version);
    output.Word(flags);
    output.Word(stageFlags);
    output.Word(requiredSubgroupSize);
    output.Word(vendorId);
    output.Word(deviceId);
    output.Word(driverVersion);
    output.Word(apiVersion);
    for (const auto value : pipelineCacheUuid) output.Word(value);
    output.String(deviceName);
    output.Word(static_cast<std::uint32_t>(sets.size()));
    for (const auto& layout : sets) {
        output.Word(layout.flags);
        output.Word(static_cast<std::uint32_t>(layout.bindings.size()));
        for (const auto& binding : layout.bindings) {
            output.Word(binding.binding);
            output.Word(static_cast<std::uint32_t>(binding.type));
            output.Word(binding.count);
            output.Word(binding.stages);
            output.Word(binding.flags);
        }
    }
    output.Word(static_cast<std::uint32_t>(pushConstants.size()));
    for (const auto& range : pushConstants) {
        output.Word(range.stageFlags);
        output.Word(range.offset);
        output.Word(range.size);
    }
    output.Word(static_cast<std::uint32_t>(specialization.size()));
    for (const auto& value : specialization) {
        output.Word(value.id);
        output.Word(value.value);
    }
    output.Word(static_cast<std::uint32_t>(spirv.size()));
    for (const auto word : spirv) output.Word(word);
    RequireCapture(output.bytes.size() <= MaxFileBytes, "encoded capture exceeds size bound");
    return std::move(output.bytes);
}

ComputePipelineCapture ComputePipelineCapture::Decode(std::span<const std::byte> bytes) {
    RequireCapture(bytes.size() <= MaxFileBytes, "record exceeds 36 MiB");
    Decoder input(bytes);
    RequireCapture(input.Word() == 0x35535041u && input.Word() == 0x504d4f43u, "invalid file signature");
    RequireCapture(input.Word() == Version, "unsupported file version");
    ComputePipelineCapture result;
    result.flags = input.Word();
    result.stageFlags = input.Word();
    result.requiredSubgroupSize = input.Word();
    result.vendorId = input.Word();
    result.deviceId = input.Word();
    result.driverVersion = input.Word();
    result.apiVersion = input.Word();
    for (auto& value : result.pipelineCacheUuid) {
        const auto word = input.Word();
        RequireCapture(word <= 255, "invalid cache UUID byte");
        value = static_cast<std::uint8_t>(word);
    }
    result.deviceName = input.String();
    result.sets.resize(input.Count(8, 8));
    std::size_t totalBindings = 0;
    for (auto& layout : result.sets) {
        layout.flags = input.Word();
        const auto count = input.Count(4096, 20);
        totalBindings += count;
        RequireCapture(totalBindings <= 4096, "too many total descriptor bindings");
        layout.bindings.resize(count);
        for (auto& binding : layout.bindings) {
            binding.binding = input.Word();
            binding.type = static_cast<VkDescriptorType>(input.Word());
            binding.count = input.Word();
            binding.stages = input.Word();
            binding.flags = input.Word();
        }
    }
    result.pushConstants.resize(input.Count(1, 12));
    for (auto& range : result.pushConstants) {
        range.stageFlags = input.Word();
        range.offset = input.Word();
        range.size = input.Word();
    }
    result.specialization.resize(input.Count(4096, 8));
    for (auto& value : result.specialization) {
        value.id = input.Word();
        value.value = input.Word();
    }
    result.spirv.resize(input.Count(MaxSpirvWords, 4));
    for (auto& word : result.spirv) word = input.Word();
    RequireCapture(input.Finished(), "trailing bytes after capture");
    result.Validate();
    return result;
}

ComputePipelineCapture ComputePipelineCapture::Read(const std::filesystem::path& path) {
    return Decode(ReadBytes(path));
}

std::string ComputePipelineCapture::Identity() const {
    return Hash(Encode());
}

std::filesystem::path ComputePipelineCapture::Write(const std::filesystem::path& directory) const {
    const auto bytes = Encode();
    const auto recordDirectory = directory / ("compute-" + Hash(bytes));
    const auto path = recordDirectory / "capture.aps5cp";
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    CheckCaptureIo(error, "cannot create capture directory");
    RequireCapture(!std::filesystem::is_symlink(CaptureStatus(directory)), "capture directory is a symbolic link");
    const auto recordStatus = CaptureStatus(recordDirectory);
    if (std::filesystem::exists(recordStatus)) {
        RequireCapture(std::filesystem::is_directory(recordStatus) && !std::filesystem::is_symlink(recordStatus), "existing capture path is not a regular capture");
        const auto captureStatus = CaptureStatus(path);
        RequireCapture(std::filesystem::is_regular_file(captureStatus) && !std::filesystem::is_symlink(captureStatus), "existing capture path is not a regular capture");
        RequireCapture(ReadBytes(path) == bytes, "existing capture differs or is incomplete");
        return path;
    }
    std::uintmax_t totalBytes = 0;
    std::size_t count = 0;
    auto iterator = std::filesystem::directory_iterator(directory, error);
    CheckCaptureIo(error, "cannot enumerate capture directory");
    while (iterator != std::filesystem::directory_iterator{}) {
        const auto& entry = *iterator;
        const auto entryStatus = CaptureStatus(entry.path());
        if (std::filesystem::is_regular_file(entryStatus) && entry.path().filename() == ".DS_Store") {
            iterator.increment(error);
            CheckCaptureIo(error, "cannot advance capture directory");
            continue;
        }
        RequireCaptureCapacity(++count < 128, "capture directory reached 128 entries");
        RequireCapture(std::filesystem::is_directory(entryStatus) && !std::filesystem::is_symlink(entryStatus), "capture directory contains an unexpected entry");
        const auto capture = entry.path() / "capture.aps5cp";
        const auto captureStatus = CaptureStatus(capture);
        RequireCapture(std::filesystem::is_regular_file(captureStatus) && !std::filesystem::is_symlink(captureStatus), "capture directory contains an incomplete record");
        const auto size = CaptureFileSize(capture);
        RequireCapture(size <= MaxFileBytes, "existing capture exceeds size bound");
        totalBytes += size;
        iterator.increment(error);
        CheckCaptureIo(error, "cannot advance capture directory");
    }
    RequireCaptureCapacity(totalBytes + bytes.size() <= 256u * 1024u * 1024u, "capture directory exceeds 256 MiB");
    const bool created = std::filesystem::create_directory(recordDirectory, error);
    CheckCaptureIo(error, "cannot create capture record directory");
    RequireCapture(created, "capture directory was concurrently created");
    std::ofstream output(path, std::ios::binary);
    RequireCaptureIo(output.good(), "cannot create capture");
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    RequireCaptureIo(output.good(), "cannot finish capture");
    return path;
}

bool ComputePipelineCapture::OptionalEnabled() {
    return !optionalDisabled.load(std::memory_order_relaxed);
}

std::filesystem::path ComputePipelineCapture::WriteOptional(const std::filesystem::path& directory) const {
    if (!OptionalEnabled()) {
        Validate();
        return {};
    }
    try {
        return Write(directory);
    } catch (const CaptureLimit& error) {
        DisableOptionalCapture(error.what());
    } catch (const CaptureIoFailure& error) {
        DisableOptionalCapture(error.what());
    }
    return {};
}

}
