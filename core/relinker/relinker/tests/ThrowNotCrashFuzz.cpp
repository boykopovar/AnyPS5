#include "ElfFixture.hpp"
#include <relinker/parsing/ElfReader.hpp>
#include <relinker/guest/GuestImage.hpp>
#include <io/BufferUtils.hpp>
#include <domain/Types.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace RelinkerTests;

constexpr std::uint64_t kSeed = 0xA13013ULL;
constexpr std::chrono::seconds kBudget(10);
constexpr std::size_t kMaxIterations = 400000;
constexpr int kProbesPerInput = 4;

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Bytes BaseFixture(std::mt19937_64& rng) {
    std::vector<std::uint8_t> code{0xC3};
    if ((rng() & 1ULL) == 0ULL) {
        return MakeExecutable(code);
    }
    return MakeModule(code);
}

void WriteU64Le(Bytes& image, const std::size_t offset, const std::uint64_t value) {
    require(offset + sizeof(value) <= image.size(), "WriteU64Le ran past the fixture");
    std::memcpy(image.data() + offset, &value, sizeof(value));
}

void WriteU32Le(Bytes& image, const std::size_t offset, const std::uint32_t value) {
    require(offset + sizeof(value) <= image.size(), "WriteU32Le ran past the fixture");
    std::memcpy(image.data() + offset, &value, sizeof(value));
}

void WriteU16Le(Bytes& image, const std::size_t offset, const std::uint16_t value) {
    require(offset + sizeof(value) <= image.size(), "WriteU16Le ran past the fixture");
    std::memcpy(image.data() + offset, &value, sizeof(value));
}

std::size_t TruncatedSize(const std::size_t full, std::mt19937_64& rng) {
    const std::size_t edges[] = {0, 1, 2, 3, 7, 15, 16, 31, 32, 63, 64, 65, 127, 128, 511, 512, 1023, 1024, 4096};
    std::uniform_int_distribution<std::size_t> coin(0, 3);
    if (coin(rng) == 0) {
        std::uniform_int_distribution<std::size_t> any(0, full);
        return any(rng);
    }
    std::uniform_int_distribution<std::size_t> pick(0, sizeof(edges) / sizeof(edges[0]) - 1);
    const std::size_t chosen = edges[pick(rng)];
    if (chosen > full) {
        return full;
    }
    return chosen;
}

void CorruptBytes(Bytes& image, std::mt19937_64& rng) {
    if (image.empty()) {
        return;
    }
    std::uniform_int_distribution<std::size_t> count(1, 16);
    std::uniform_int_distribution<std::size_t> where(0, image.size() - 1);
    std::uniform_int_distribution<unsigned> what(0, 255);
    const std::size_t rounds = count(rng);
    for (std::size_t index = 0; index < rounds; ++index) {
        image[where(rng)] = static_cast<std::uint8_t>(what(rng));
    }
}

void CorruptTable(Bytes& image, std::mt19937_64& rng) {
    if (image.size() < 64) {
        CorruptBytes(image, rng);
        return;
    }
    const std::size_t fields[] = {16, 18, 20, 24, 32, 40, 48, 52, 54, 56, 58, 60, 62, 64, 72, 80, 120, 128};
    std::uniform_int_distribution<std::size_t> pick(0, sizeof(fields) / sizeof(fields[0]) - 1);
    const std::size_t offset = fields[pick(rng)];
    const std::uint64_t extremes[] = {0, 1, 2, 7, 15, 16, 24, 56, 64, 255, 4096, 65535, 4294967295ULL, 18446744073709551615ULL, 18446744073709551608ULL, 9223372036854775807ULL};
    std::uniform_int_distribution<std::size_t> which(0, sizeof(extremes) / sizeof(extremes[0]) - 1);
    std::uniform_int_distribution<std::uint64_t> randomValue(0, 18446744073709551615ULL);
    std::uint64_t value = extremes[which(rng)];
    if (which(rng) == 0) {
        value = randomValue(rng);
    }
    std::uniform_int_distribution<int> width(0, 2);
    const int kind = width(rng);
    if (kind == 0 && offset + 2 <= image.size()) {
        WriteU16Le(image, offset, static_cast<std::uint16_t>(value & 65535ULL));
    } else if (kind == 1 && offset + 4 <= image.size()) {
        WriteU32Le(image, offset, static_cast<std::uint32_t>(value & 4294967295ULL));
    } else if (offset + 8 <= image.size()) {
        WriteU64Le(image, offset, value);
    } else {
        CorruptBytes(image, rng);
    }
}

Bytes MutateInput(Bytes base, std::mt19937_64& rng) {
    std::uniform_int_distribution<unsigned> pristine(0, 19);
    if (pristine(rng) == 0) {
        return base;
    }
    std::uniform_int_distribution<unsigned> kind(0, 2);
    const unsigned chosen = kind(rng);
    if (chosen == 0) {
        base.resize(TruncatedSize(base.size(), rng));
    } else if (chosen == 1) {
        CorruptBytes(base, rng);
    } else {
        CorruptTable(base, rng);
    }
    std::uniform_int_distribution<unsigned> extra(0, 3);
    if (!base.empty() && extra(rng) == 0) {
        CorruptBytes(base, rng);
    }
    return base;
}

void ExerciseElfReader(const Bytes& bytes, std::mt19937_64& rng) {
    Relinker::ElfReader reader(bytes);
    const Domain::ElfHeader header = reader.ReadHeader();
    const std::vector<Domain::ProgramHeader> programs = reader.ReadProgramHeaders();
    const std::vector<Domain::SectionHeader> sections = reader.ReadSectionHeaders();
    const std::vector<Domain::ProgramHeader> code = reader.ReadCodeSegments();
    const std::uint64_t fileSize = reader.GetFileSize();
    require(fileSize == bytes.size(), "ElfReader misreported the file size");
    const std::vector<std::uint8_t>& raw = reader.GetRawBytes();
    require(raw == bytes, "ElfReader misreported the raw bytes");
    std::vector<std::uint64_t> translated;
    translated.push_back(reader.TranslateVirtualAddress(header.EntryPoint));
    translated.push_back(reader.TranslateVirtualAddress(0));
    require(translated.size() == 2, "ElfReader lost a translated address");
    std::uniform_int_distribution<std::uint64_t> address(0, 18446744073709551615ULL);
    for (int index = 0; index < kProbesPerInput; ++index) {
        try {
            translated.push_back(reader.TranslateVirtualAddress(address(rng)));
        } catch (const Domain::RelinkerException& failure) {
            require(failure.what() != nullptr, "ElfReader hid an empty random diagnostic");
        }
    }
    require(translated.size() >= 2, "ElfReader lost a translated address");
    std::size_t segmentBytes = 0;
    for (const auto& program : programs) {
        const std::vector<std::uint8_t> part = reader.ReadSegment(program);
        segmentBytes += part.size();
        require(part.size() <= bytes.size(), "ElfReader returned a segment larger than the file");
        if (program.Type == 2) {
            const std::vector<Domain::DynamicTag> tags = reader.ReadDynamicTags(program);
            require(tags.size() <= bytes.size(), "ElfReader returned more tags than bytes");
        }
    }
    std::size_t sectionBytes = 0;
    for (const auto& section : sections) {
        const std::vector<std::uint8_t> part = reader.ReadSection(section);
        sectionBytes += part.size();
        require(part.size() <= bytes.size(), "ElfReader returned a section larger than the file");
    }
    require(segmentBytes <= bytes.size() * (programs.size() + 1), "ElfReader segment accounting overflowed");
    require(sectionBytes <= bytes.size() * (sections.size() + 1), "ElfReader section accounting overflowed");
    require(code.size() <= programs.size(), "ElfReader returned more code segments than program headers");
    Domain::ProgramHeader synthetic{};
    synthetic.Type = 2;
    synthetic.Offset = 18446744073709551615ULL;
    synthetic.FileSize = 16;
    bool syntheticRejected = false;
    try {
        const std::vector<std::uint8_t> part = reader.ReadSegment(synthetic);
        require(part.size() <= bytes.size() + 16, "ElfReader synthetic probe overflowed");
    } catch (const Domain::RelinkerException& failure) {
        syntheticRejected = failure.what() != nullptr;
        require(syntheticRejected, "ElfReader hid an empty synthetic diagnostic");
    }
    try {
        const std::vector<Domain::DynamicTag> tags = reader.ReadDynamicTags(synthetic);
        require(tags.size() <= bytes.size() + 1, "ElfReader synthetic dynamic probe overflowed");
    } catch (const Domain::RelinkerException& failure) {
        syntheticRejected = failure.what() != nullptr;
        require(syntheticRejected, "ElfReader hid an empty dynamic diagnostic");
    }
}

void ExerciseGuestImage(const Bytes& bytes) {
    const Relinker::GuestImageReader reader;
    Relinker::GuestImage image = reader.Read(std::filesystem::path("fuzz.elf"), bytes);
    require(image.Bytes.size() == bytes.size(), "GuestImageReader lost the image bytes");
    require(image.Dependencies.size() <= bytes.size(), "GuestImageReader returned more dependencies than bytes");
}

void ExerciseBufferUtils(const Bytes& bytes, std::mt19937_64& rng) {
    std::uniform_int_distribution<std::size_t> offsetPick(0, bytes.size() + 8);
    std::uniform_int_distribution<std::uint64_t> valuePick(0, 18446744073709551615ULL);
    for (int round = 0; round < kProbesPerInput; ++round) {
        const std::size_t offset = offsetPick(rng);
        try {
            const std::uint64_t first = Io::ReadU16(bytes, offset);
            const std::uint64_t second = Io::ReadU32(bytes, offset);
            const std::uint64_t third = Io::ReadU64(bytes, offset);
            const std::vector<std::uint64_t> observed{first, second, third};
            require(observed.size() == 3, "BufferUtils read probe lost data");
        } catch (const std::out_of_range& failure) {
            require(failure.what() != nullptr, "BufferUtils hid an empty read diagnostic");
        }
        try {
            Bytes scratch = bytes;
            const std::size_t safe = offset % (scratch.size() + 1);
            const std::uint64_t value = valuePick(rng);
            if (safe < scratch.size()) {
                Io::WriteU8(scratch, safe, static_cast<std::uint8_t>(value & 255ULL));
            } else {
                Io::WriteU8(scratch, safe, static_cast<std::uint8_t>(value & 255ULL));
            }
            require(scratch.size() == bytes.size(), "BufferUtils write changed the size");
        } catch (const std::out_of_range& failure) {
            require(failure.what() != nullptr, "BufferUtils hid an empty write diagnostic");
        }
        try {
            Bytes scratch = bytes;
            Io::WriteU16(scratch, offset % (scratch.size() + 1), static_cast<std::uint16_t>(valuePick(rng) & 65535ULL));
            Io::WriteU32(scratch, offset % (scratch.size() + 1), static_cast<std::uint32_t>(valuePick(rng) & 4294967295ULL));
            Io::WriteU64(scratch, offset % (scratch.size() + 1), valuePick(rng));
            require(scratch.size() == bytes.size(), "BufferUtils wide write changed the size");
        } catch (const std::out_of_range& failure) {
            require(failure.what() != nullptr, "BufferUtils hid an empty wide diagnostic");
        }
        try {
            const std::uint64_t value = valuePick(rng);
            const std::uint64_t alignment = 1 + (valuePick(rng) % 4096);
            const std::uint64_t up = Io::AlignUp64(value, alignment);
            require(up >= value, "AlignUp64 moved backwards");
        } catch (const std::overflow_error& failure) {
            require(failure.what() != nullptr, "AlignUp64 hid an empty overflow diagnostic");
        } catch (const std::invalid_argument& failure) {
            require(failure.what() != nullptr, "AlignUp64 hid an empty argument diagnostic");
        }
    }
    try {
        Bytes scratch = bytes;
        Io::AlignBuffer(scratch, 0);
        require(false, "AlignBuffer accepted zero alignment");
    } catch (const std::invalid_argument& failure) {
        require(failure.what() != nullptr, "AlignBuffer hid an empty diagnostic");
    }
    try {
        const std::uint64_t up = Io::AlignUp64(16, 0);
        require(up + 1 == up, "AlignUp64 accepted zero alignment");
    } catch (const std::invalid_argument& failure) {
        require(failure.what() != nullptr, "AlignUp64 hid an empty zero diagnostic");
    }
    Bytes appendScratch;
    Io::AppendU8(appendScratch, 171);
    Io::AppendU16(appendScratch, 4660);
    Io::AppendU32(appendScratch, 2309737967UL);
    Io::AppendU64(appendScratch, 123456789ULL);
    Io::AppendI64(appendScratch, -42);
    require(appendScratch.size() == 23, "BufferUtils append lost bytes");
    Io::AppendString(appendScratch, "guest");
    require(!appendScratch.empty() && appendScratch.back() == 0, "BufferUtils string append lost its terminator");
    Io::AlignBuffer(appendScratch, 8);
    require(appendScratch.size() % 8 == 0, "BufferUtils align lost its padding");
}

bool TryElfReader(const Bytes& bytes, std::mt19937_64& rng) {
    try {
        ExerciseElfReader(bytes, rng);
        return true;
    } catch (const Domain::RelinkerException&) {
        return false;
    }
}

bool TryGuestImage(const Bytes& bytes) {
    try {
        ExerciseGuestImage(bytes);
        return true;
    } catch (const Domain::RelinkerException&) {
        return false;
    }
}

}

int main() {
    try {
        std::mt19937_64 rng(kSeed);
        const auto start = std::chrono::steady_clock::now();
        std::size_t accepted = 0;
        std::size_t rejected = 0;
        std::size_t iteration = 0;
        while (iteration < kMaxIterations) {
            const auto now = std::chrono::steady_clock::now();
            if (now - start >= kBudget) {
                break;
            }
            Bytes base = BaseFixture(rng);
            Bytes input = MutateInput(std::move(base), rng);
            const bool elfConverted = TryElfReader(input, rng);
            const bool guestConverted = TryGuestImage(input);
            ExerciseBufferUtils(input, rng);
            if (elfConverted && guestConverted) {
                ++accepted;
            } else {
                ++rejected;
            }
            ++iteration;
        }
        require(iteration > 0, "Fuzz loop executed no iterations");
        const auto elapsed = std::chrono::steady_clock::now() - start;
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        std::cout << "throw_not_crash_fuzz: " << iteration << " inputs (" << accepted << " converted, " << rejected << " rejected) in " << seconds << "s" << '\n';
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "throw_not_crash_fuzz failed: " << failure.what() << '\n';
        return 1;
    }
}
