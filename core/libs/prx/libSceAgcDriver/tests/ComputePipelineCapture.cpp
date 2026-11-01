#include "../tools/ComputePipelineFixture.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void Reject(TAction action, const char* message) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, message);
}

template<typename TAction>
void RejectWithMessage(TAction action, const char* expected) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception& error) {
        rejected = std::string_view(error.what()).find(expected) != std::string_view::npos;
    }
    Require(rejected, "strict capture did not preserve its diagnostic");
}

class Directory {
public:
    Directory() {
        path = std::filesystem::temp_directory_path() / ("anyps5-compute-capture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(path), "could not create test directory");
    }
    ~Directory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

void CreateSizedFile(const std::filesystem::path& path, std::uintmax_t bytes) {
#ifdef _WIN32
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(file != INVALID_HANDLE_VALUE, "cannot create sized file");
    LARGE_INTEGER size{};
    size.QuadPart = static_cast<LONGLONG>(bytes);
    const bool sized = SetFilePointerEx(file, size, nullptr, FILE_BEGIN) && SetEndOfFile(file);
    CloseHandle(file);
    Require(sized, "cannot size file");
#else
    Require(std::ofstream(path, std::ios::binary).good(), "cannot create sized file");
    std::filesystem::resize_file(path, bytes);
#endif
}

void CheckRoundTrip() {
    auto capture = AgcDriver::ComputePipelineFixture();
    capture.deviceName = "test GPU \"name\"";
    capture.vendorId = 0x10de;
    capture.deviceId = 0x2b85;
    capture.driverVersion = 0x12345678;
    capture.apiVersion = VK_API_VERSION_1_3;
    capture.pipelineCacheUuid[0] = 0x12;
    capture.pipelineCacheUuid[15] = 0xab;
    capture.requiredSubgroupSize = 32;
    capture.flags = VK_PIPELINE_CREATE_DISPATCH_BASE_BIT;
    capture.sets = {{VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                     {{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT},
                      {7, VK_DESCRIPTOR_TYPE_SAMPLER, 2, VK_SHADER_STAGE_COMPUTE_BIT, 0}}}};
    capture.pushConstants = {{VK_SHADER_STAGE_COMPUTE_BIT, 16, 32}};
    const auto encoded = capture.Encode();
    const auto decoded = AgcDriver::ComputePipelineCapture::Decode(encoded);
    Require(decoded.Encode() == encoded, "capture round trip changed pipeline inputs");
    Require(decoded.Identity() == capture.Identity(), "capture identity changed after decoding");
    auto changed = decoded;
    ++changed.specialization[0].value;
    Require(changed.Identity() != capture.Identity(), "specialization did not affect capture identity");
    changed = decoded;
    ++changed.sets[0].bindings[0].count;
    Require(changed.Identity() != capture.Identity(), "descriptor layout did not affect capture identity");
    changed = decoded;
    changed.requiredSubgroupSize = 64;
    Require(changed.Identity() != capture.Identity(), "subgroup size did not affect capture identity");
    changed = decoded;
    changed.stageFlags = VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
    Require(AgcDriver::ComputePipelineCapture::Decode(changed.Encode()).stageFlags == changed.stageFlags, "full subgroups were lost in the capture");
    Require(changed.Identity() != capture.Identity(), "full subgroups did not affect capture identity");
    Directory directory;
    const auto file = capture.Write(directory.path);
    Require(AgcDriver::ComputePipelineCapture::Read(file).Encode() == encoded, "capture file changed pipeline inputs");
    Require(capture.Write(directory.path) == file, "identical capture was not deduplicated");
    std::ofstream(file, std::ios::binary | std::ios::trunc).put('x');
    Reject([&] { capture.Write(directory.path); }, "existing damaged capture was overwritten");
    Require(std::filesystem::file_size(file) == 1, "damaged capture was changed");
}

void CheckLargeRecord() {
    auto capture = AgcDriver::ComputePipelineFixture();
    constexpr std::size_t words = 5500000;
    capture.spirv.insert(capture.spirv.end() - 2, words - capture.spirv.size(), 0x00010000u);
    const auto encoded = capture.Encode();
    Require(encoded.size() > 20u * 1024u * 1024u, "large fixture did not exceed the former file bound");
    Require(AgcDriver::ComputePipelineCapture::Decode(encoded).spirv == capture.spirv, "large module changed during decoding");
    Directory directory;
    const auto file = capture.Write(directory.path);
    Require(AgcDriver::ComputePipelineCapture::Read(file).spirv == capture.spirv, "large module changed on disk");
    capture.spirv.resize(AgcDriver::ComputePipelineCapture::MaxSpirvWords + 1u, 0x00010000u);
    Reject([&] { capture.Encode(); }, "module beyond the word bound accepted");
    std::vector<std::byte> oversized(AgcDriver::ComputePipelineCapture::MaxFileBytes + 1u);
    Reject([&] { AgcDriver::ComputePipelineCapture::Decode(oversized); }, "record beyond the file bound accepted");
    const auto oversizedFile = directory.path / "oversized.aps5cp";
    {
        std::ofstream output(oversizedFile, std::ios::binary);
        output.seekp(static_cast<std::streamoff>(AgcDriver::ComputePipelineCapture::MaxFileBytes));
        output.put('x');
        Require(output.good(), "cannot write oversized capture fixture");
    }
    Reject([&] { AgcDriver::ComputePipelineCapture::Read(oversizedFile); }, "file beyond the file bound accepted");
}

void CheckBindingFlags() {
    auto capture = AgcDriver::ComputePipelineFixture();
    capture.sets = {{0, {{7, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT},
                         {3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT}}}};
    const auto encoded = capture.Encode();
    Require(AgcDriver::ComputePipelineCapture::Decode(encoded).Encode() == encoded, "binding flags changed without update-after-bind");
    auto changed = capture;
    changed.sets[0].bindings[0].flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    Require(changed.Identity() != capture.Identity(), "binding flags did not affect capture identity");
    changed = capture;
    changed.sets[0].bindings[1].flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    Reject([&] { changed.Encode(); }, "non-highest variable descriptor binding accepted");
    changed = capture;
    changed.sets[0].bindings[0].flags |= VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    Reject([&] { changed.Encode(); }, "unsupported binding flags accepted");
    capture.sets[0].flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    capture.sets[0].bindings[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    capture.sets[0].bindings[0].flags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    Require(AgcDriver::ComputePipelineCapture::Decode(capture.Encode()).Encode() == capture.Encode(), "combined binding flags changed");
}

void CheckMalformedRecords() {
    const auto capture = AgcDriver::ComputePipelineFixture();
    const auto encoded = capture.Encode();
    for (std::size_t length = 0; length < encoded.size(); ++length) {
        Reject([&] { AgcDriver::ComputePipelineCapture::Decode(std::span(encoded).first(length)); }, "truncated capture accepted");
    }
    auto bytes = encoded;
    bytes.push_back(std::byte{0});
    Reject([&] { AgcDriver::ComputePipelineCapture::Decode(bytes); }, "trailing capture bytes accepted");
    bytes = encoded;
    bytes[0] ^= std::byte{1};
    Reject([&] { AgcDriver::ComputePipelineCapture::Decode(bytes); }, "invalid signature accepted");
    bytes = encoded;
    bytes[8] = std::byte{0xff};
    Reject([&] { AgcDriver::ComputePipelineCapture::Decode(bytes); }, "unknown capture version accepted");
    bytes = encoded;
    for (std::size_t index = 104; index < 108; ++index) bytes[index] = std::byte{0xff};
    Reject([&] { AgcDriver::ComputePipelineCapture::Decode(bytes); }, "unbounded device name accepted");
    const auto invalid = [&](auto change) {
        auto copy = capture;
        change(copy);
        Reject([&] { copy.Encode(); }, "unsupported pipeline input accepted");
    };
    invalid([](auto& copy) { copy.spirv[0] = 0; });
    invalid([](auto& copy) { copy.spirv[5] = 0; });
    invalid([](auto& copy) { copy.spirv[11] = 4; });
    invalid([](auto& copy) { copy.spirv[13] = 0; });
    invalid([](auto& copy) { copy.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT; });
    invalid([](auto& copy) { copy.stageFlags = VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT; });
    invalid([](auto& copy) {
        copy.stageFlags = VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
        copy.requiredSubgroupSize = 0;
    });
    invalid([](auto& copy) { copy.requiredSubgroupSize = 3; });
    invalid([](auto& copy) { copy.specialization.push_back(copy.specialization[0]); });
    invalid([](auto& copy) { copy.pushConstants = {{VK_SHADER_STAGE_VERTEX_BIT, 0, 4}}; });
    invalid([](auto& copy) { copy.pushConstants = {{VK_SHADER_STAGE_COMPUTE_BIT, 1, 4}}; });
    invalid([](auto& copy) { copy.sets = {{0, {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, VK_SHADER_STAGE_COMPUTE_BIT, 0}}}}; });
    invalid([](auto& copy) { copy.sets = {{0, {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, 0}}}}; });
    invalid([](auto& copy) { copy.sets = {{0, {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT}}}}; });
    invalid([](auto& copy) { copy.sets = {{0, {{}, {}}}}; });
}

void CheckOptionalEntryCap() {
    Directory directory;
    auto capture = AgcDriver::ComputePipelineFixture();
    std::filesystem::path first;
    for (std::uint32_t index = 0; index < 128u; ++index) {
        capture.specialization[0].value = index;
        const auto path = capture.WriteOptional(directory.path);
        Require(!path.empty() && AgcDriver::ComputePipelineCapture::OptionalEnabled(), "optional capture stopped before accepting 128 records");
        if (index == 0u) first = path;
    }
    capture.specialization[0].value = 0u;
    Require(capture.WriteOptional(directory.path) == first, "optional capture did not deduplicate at the entry cap");
    capture.specialization[0].value = 128u;
    Require(capture.WriteOptional(directory.path).empty() && !AgcDriver::ComputePipelineCapture::OptionalEnabled(), "optional capture did not stop at the entry cap");
    RejectWithMessage([&] { capture.Write(directory.path); }, "compute pipeline capture: capture directory reached 128 entries");
    for (unsigned index = 0; index < 64u; ++index) Require(capture.WriteOptional(directory.path / "after-stop").empty(), "disabled optional capture wrote another file");
    Require(!std::filesystem::exists(directory.path / "after-stop"), "disabled optional capture touched its output directory");
    capture.specialization[0].value = 0u;
    Require(capture.Write(directory.path) == first, "optional stop disabled strict capture deduplication");
}

void CheckOptionalByteCap() {
    Directory directory;
    auto capture = AgcDriver::ComputePipelineFixture();
    const auto smallBytes = capture.Encode().size();
    constexpr std::uintmax_t chunkBytes = 32u * 1024u * 1024u;
    for (std::uint32_t index = 0; index < 8u; ++index) {
        const auto record = directory.path / ("compute-" + std::to_string(index));
        Require(std::filesystem::create_directory(record), "cannot create byte-cap fixture record");
        CreateSizedFile(record / "capture.aps5cp", chunkBytes - (index == 7u ? smallBytes : 0u));
    }
    capture.specialization[0].value = 8u;
    Require(!capture.WriteOptional(directory.path).empty() && AgcDriver::ComputePipelineCapture::OptionalEnabled(), "optional capture refused exactly 256 MiB");
    std::uintmax_t total = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path)) total += std::filesystem::file_size(entry.path() / "capture.aps5cp");
    Require(total == 256u * 1024u * 1024u, "byte-cap fixture did not reach exactly 256 MiB");
    capture.specialization[0].value = 9u;
    Require(capture.WriteOptional(directory.path).empty() && !AgcDriver::ComputePipelineCapture::OptionalEnabled(), "optional capture did not stop beyond the byte cap");
    RejectWithMessage([&] { capture.Write(directory.path); }, "compute pipeline capture: capture directory exceeds 256 MiB");
}

void CheckOptionalIo() {
    Directory directory;
    const auto blocked = directory.path / "blocked";
    {
        std::ofstream output(blocked, std::ios::binary);
        output.put('x');
        Require(output.good(), "cannot create output conflict");
    }
    const auto capture = AgcDriver::ComputePipelineFixture();
    Require(capture.WriteOptional(blocked / "captures").empty() && !AgcDriver::ComputePipelineCapture::OptionalEnabled(), "optional capture did not stop after an output I/O error");
    Require(std::filesystem::file_size(blocked) == 1u, "optional capture changed the conflicting file");
    RejectWithMessage([&] { capture.Write(blocked / "captures"); }, "compute pipeline capture: cannot create capture directory");
    Require(capture.WriteOptional(directory.path / "after-stop").empty() && !std::filesystem::exists(directory.path / "after-stop"), "disabled optional capture retried output");
    auto malformed = capture;
    malformed.spirv[0] = 0u;
    Reject([&] { malformed.WriteOptional(directory.path / "after-stop"); }, "disabled optional capture swallowed an invalid pipeline input");
}

void CheckOptionalInvalid() {
    Directory directory;
    auto capture = AgcDriver::ComputePipelineFixture();
    auto malformed = capture;
    malformed.spirv[0] = 0u;
    Reject([&] { malformed.WriteOptional(directory.path / "malformed"); }, "optional capture swallowed an invalid pipeline input");
    Require(AgcDriver::ComputePipelineCapture::OptionalEnabled() && !std::filesystem::exists(directory.path / "malformed"), "invalid input disabled optional capture or touched its output");
    const auto path = capture.WriteOptional(directory.path);
    Require(!path.empty(), "optional capture refused valid input");
    std::ofstream(path, std::ios::binary | std::ios::trunc).put('x');
    Reject([&] { capture.WriteOptional(directory.path); }, "optional capture swallowed an existing damaged record");
    Require(AgcDriver::ComputePipelineCapture::OptionalEnabled() && std::filesystem::file_size(path) == 1u, "damaged record disabled optional capture or was overwritten");
    Require(std::filesystem::remove(path), "cannot remove owned damaged record");
    Reject([&] { capture.WriteOptional(directory.path); }, "optional capture swallowed an incomplete existing record");
    Require(AgcDriver::ComputePipelineCapture::OptionalEnabled() && !std::filesystem::exists(path), "incomplete record disabled optional capture or was replaced");
    Require(std::filesystem::create_directory(path), "cannot create owned nonregular capture");
    Reject([&] { capture.WriteOptional(directory.path); }, "optional capture swallowed a nonregular existing capture");
    Require(AgcDriver::ComputePipelineCapture::OptionalEnabled() && std::filesystem::is_directory(path), "nonregular capture disabled optional capture or was changed");
    std::filesystem::remove_all(path.parent_path());
    std::ofstream(path.parent_path(), std::ios::binary).put('x');
    Reject([&] { capture.WriteOptional(directory.path); }, "optional capture swallowed a non-directory existing record");
    Require(AgcDriver::ComputePipelineCapture::OptionalEnabled() && std::filesystem::file_size(path.parent_path()) == 1u, "non-directory record disabled optional capture or was changed");
    Directory unexpected;
    const auto finder = unexpected.path / ".DS_Store";
    std::ofstream(finder, std::ios::binary).put('x');
    capture.specialization[0].value = 1u;
    Require(!capture.Write(unexpected.path).empty(), "strict capture rejected a Finder metadata file");
    capture.specialization[0].value = 2u;
    Require(!capture.WriteOptional(unexpected.path).empty() && AgcDriver::ComputePipelineCapture::OptionalEnabled(), "optional capture rejected a Finder metadata file");
    Require(std::filesystem::file_size(finder) == 1u, "Finder metadata file was changed");
    capture.specialization[0].value = 3u;
    const auto foreign = unexpected.path / "foreign";
    std::ofstream(foreign, std::ios::binary).put('x');
    RejectWithMessage([&] { capture.Write(unexpected.path); }, "capture directory contains an unexpected entry");
    Reject([&] { capture.WriteOptional(unexpected.path); }, "optional capture swallowed an unexpected directory entry");
    Require(AgcDriver::ComputePipelineCapture::OptionalEnabled() && std::filesystem::file_size(foreign) == 1u, "unexpected entry disabled optional capture or was changed");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            const std::string_view mode = argv[1];
            if (mode == "--optional-entry-cap") CheckOptionalEntryCap();
            else if (mode == "--optional-byte-cap") CheckOptionalByteCap();
            else if (mode == "--optional-io") CheckOptionalIo();
            else if (mode == "--optional-invalid") CheckOptionalInvalid();
            else throw std::runtime_error("unknown optional capture test mode");
            std::puts("optional compute capture policy passed");
            return 0;
        }
        Require(argc == 1, "unknown capture test arguments");
        CheckRoundTrip();
        CheckMalformedRecords();
        CheckBindingFlags();
        CheckLargeRecord();
        std::puts("compute pipeline capture: round trip, 5500000-word module, binding flags, immutable writes and bounded inputs passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "compute pipeline capture test: %s\n", error.what());
        return 1;
    }
}
