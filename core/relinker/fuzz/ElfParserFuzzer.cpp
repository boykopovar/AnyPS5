#include <relinker/parsing/ElfReader.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 4) {
        return 0;
    }

    try {
        const std::vector<std::uint8_t> buffer(data, data + size);
        const Relinker::ElfReader reader(buffer);

        reader.ValidateMagic();
        const auto header = reader.ReadHeader();
        const auto programHeaders = reader.ReadProgramHeaders();
        const auto sectionHeaders = reader.ReadSectionHeaders();

        for (const auto& ph : programHeaders) {
            if (ph.Type == 2) {
                reader.ReadDynamicTags(ph);
            }
            if (ph.Type == 1 && ph.FileSize > 0 && ph.FileSize <= 0x10000) {
                reader.ReadSegment(ph);
            }
        }

        if (!programHeaders.empty()) {
            const auto targetAddress = programHeaders[0].MappedAddress;
            reader.TranslateVirtualAddress(targetAddress);
        }

        if (size >= 8) {
            std::uint64_t arbitraryAddress = 0;
            for (std::size_t i = 0; i < 8; ++i) {
                arbitraryAddress |= static_cast<std::uint64_t>(data[i]) << (i * 8);
            }
            try {
                reader.TranslateVirtualAddress(arbitraryAddress);
            } catch (const Relinker::RelinkerException&) {
            }
        }
    } catch (const Relinker::RelinkerException&) {
    } catch (const std::exception&) {
    }

    return 0;
}
