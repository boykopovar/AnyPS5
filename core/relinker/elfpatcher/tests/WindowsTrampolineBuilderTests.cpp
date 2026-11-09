#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <elfpatcher/windows/WindowsTrampolineBuilder.hpp>
#include <codegen/CodegenTypes.hpp>
#include <domain/Types.hpp>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Elfpatcher::Windows;
using Bytes = std::vector<std::uint8_t>;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TValue>
void write(Bytes& bytes, const std::size_t offset, const TValue value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

const Bytes SiteBytes{0xF2, 0x0F, 0x78, 0xDB, 0x08, 0x08};
const Bytes StubBody{0x90, 0xE9, 0x00, 0x00, 0x00, 0x00};

Bytes elfFixture() {
    Bytes bytes(0x400);
    bytes[0] = 0x7F;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = 1;
    write<std::uint16_t>(bytes, 16, 3);
    write<std::uint16_t>(bytes, 18, 62);
    write<std::uint64_t>(bytes, 24, 0x1000);
    std::fill(bytes.begin() + 0x200, bytes.begin() + 0x300, 0xCC);
    std::copy(SiteBytes.begin(), SiteBytes.end(), bytes.begin() + 0x202);
    std::copy(SiteBytes.begin(), SiteBytes.end(), bytes.begin() + 0x210);
    std::copy(SiteBytes.begin(), SiteBytes.end(), bytes.begin() + 0x302);
    return bytes;
}

std::vector<Domain::ProgramHeader> elfHeaders() {
    return {{1, 5, 0x200, 0x1000, 0, 0x100, 0x100, 0x1000}, {1, 6, 0x300, 0x2000, 0, 0x100, 0x100, 0x1000}};
}

const Bytes Source = elfFixture();

Codegen::TrampolineSite site(const std::uint64_t address) {
    const auto offset = static_cast<std::ptrdiff_t>(address & 0xFFF) + (address < 0x2000 ? 0x200 : 0x300);
    return {static_cast<std::uint64_t>(offset), address, SiteBytes.size(), {Source.begin() + offset, Source.begin() + offset + static_cast<std::ptrdiff_t>(SiteBytes.size())}, StubBody, 1};
}

void requireRejected(const WindowsLoadImage& image, const std::vector<Codegen::TrampolineSite>& sites, std::vector<PeSection> sections, std::uint32_t nextRva, const std::string& expected) {
    const auto original = sections;
    const auto originalRva = nextRva;
    try {
        WindowsTrampolineBuilder().Build(sites, image, sections, nextRva);
    } catch (const Domain::RelinkerException& error) {
        require(std::string(error.what()).find(expected) != std::string::npos, "Unexpected error '" + std::string(error.what()) + "', expected '" + expected + "'");
        require(sections.size() == original.size() && nextRva == originalRva, "Rejected build changed the section list for '" + expected + "'");
        for (std::size_t index = 0; index < sections.size(); ++index)
            require(sections[index].Data == original[index].Data, "Rejected build patched section " + sections[index].Name + " for '" + expected + "'");
        return;
    }
    throw std::runtime_error("Build succeeded, expected '" + expected + "'");
}

void run() {
    const WindowsLoadImage image(Source, elfHeaders());
    const auto endRva = image.GetEndRva();

    auto sections = image.BuildSections();
    auto nextRva = endRva;
    WindowsTrampolineBuilder().Build({site(0x1002), site(0x1010)}, image, sections, nextRva);
    require(sections.size() == 3 && sections.back().Name == ".amdstub" && sections.back().Rva == endRva, "Stub section was not appended");
    require(nextRva == AlignRva(static_cast<std::uint64_t>(endRva) + sections.back().Data.size()), "Next RVA does not follow the stub section");
    require(sections[0].Data[2] == 0xE9 && sections[0].Data[7] == 0x90 && sections[0].Data[0x10] == 0xE9 && sections[0].Data[0x15] == 0x90, "Sites were not replaced by jumps");

    const auto fresh = image.BuildSections();
    requireRejected(image, {site(0x1002), site(0x1004)}, fresh, endRva, "Overlapping AMD-only trampoline sites");
    requireRejected(image, {site(0x1004), site(0x1002)}, fresh, endRva, "Overlapping AMD-only trampoline sites");
    requireRejected(image, {site(0x1002), site(0x1002)}, fresh, endRva, "Overlapping AMD-only trampoline sites");
    requireRejected(image, {site(0x2002)}, fresh, endRva, "AMD-only instruction is in a non-executable PE section");

    auto changed = site(0x1010);
    changed.OriginalBytes[3] = 0xDC;
    requireRejected(image, {site(0x1002), changed}, fresh, endRva, "AMD-only site bytes changed before patching");

    requireRejected(image, {site(0x1002)}, fresh, endRva + 0x10, "Invalid Windows trampoline section RVA");
    requireRejected(image, {site(0x1002)}, fresh, LoadRva - SectionAlignment, "Invalid Windows trampoline section RVA");
    requireRejected(image, {site(0x1002)}, fresh, endRva - SectionAlignment, "Windows trampoline section overlaps preceding section");
    requireRejected(image, {site(0x1002)}, sections, nextRva, "Duplicate Windows trampoline section");

    auto untouched = image.BuildSections();
    auto untouchedRva = endRva;
    WindowsTrampolineBuilder().Build({}, image, untouched, untouchedRva);
    require(untouched.size() == fresh.size() && untouchedRva == endRva, "Empty site list changed the image");
}

}

int main() {
    try {
        run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
