#include <relinker/analysis/UnusedNidFilter/IEntryPointCollector.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <cstring>

namespace Relinker::UnusedNidFilter {

using namespace Elfpatcher;

namespace {

std::uint16_t read16(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint16_t v = 0;
    std::memcpy(&v, b.data() + off, 2);
    return v;
}

std::uint32_t read32(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + off, 4);
    return v;
}

std::uint64_t read64(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint64_t v = 0;
    std::memcpy(&v, b.data() + off, 8);
    return v;
}

bool fitsAt(std::uint64_t base, std::uint64_t offset, std::uint64_t length, std::size_t size) {
    return base <= size && offset <= size - base && length <= size - base - offset;
}

bool inText(VirtualAddress va, VirtualAddress textVaddr, std::size_t textSize) {
    return va >= textVaddr && va < textVaddr + static_cast<VirtualAddress>(textSize);
}

}

class EntryPointCollector : public IEntryPointCollector {
public:
    std::vector<VirtualAddress> Collect(
        const std::vector<std::uint8_t>& elfBytes,
        VirtualAddress textVaddr,
        std::size_t textSize
    ) const override {
        if (elfBytes.size() < 64) throw RelinkerException("ELF too small for header");
        if (elfBytes[0] != 0x7f || elfBytes[1] != 'E' ||
            elfBytes[2] != 'L' || elfBytes[3] != 'F') {
            throw RelinkerException("Not an ELF file");
        }
        if (elfBytes[4] != 2) throw RelinkerException("Only ELF64 supported");

        std::vector<VirtualAddress> entries;

        auto addIfInText = [&](VirtualAddress va) {
            if (va != 0 && inText(va, textVaddr, textSize))
                entries.push_back(va);
        };

        VirtualAddress elfEntry = read64(elfBytes, 24);
        addIfInText(elfEntry);

        std::uint64_t phOff = read64(elfBytes, 32);
        std::uint16_t phEntSize = read16(elfBytes, 54);
        std::uint16_t phCount = read16(elfBytes, 56);

        if (phEntSize < 56) throw RelinkerException("ELF program header entry too small");

        for (std::uint16_t i = 0; i < phCount; ++i) {
            const std::uint64_t phIndexOff = static_cast<std::uint64_t>(i) * phEntSize;
            if (!fitsAt(phOff, phIndexOff, 56, elfBytes.size())) throw RelinkerException("Program header out of bounds");
            std::size_t phPos = static_cast<std::size_t>(phOff + phIndexOff);

            std::uint32_t type = read32(elfBytes, phPos);

            if (type != PT_DYNAMIC) continue;

            std::uint64_t segOff = read64(elfBytes, phPos + 8);
            std::uint64_t segSz = read64(elfBytes, phPos + 32);

            if (!fitsAt(segOff, 0, segSz, elfBytes.size())) throw RelinkerException("PT_DYNAMIC segment out of bounds");

            VirtualAddress initArrayVa = 0;
            std::uint64_t initArraySz = 0;
            VirtualAddress finiArrayVa = 0;
            std::uint64_t finiArraySz = 0;

            for (std::uint64_t off = 0; off + 16 <= segSz; off += 16) {
                std::size_t pos = static_cast<std::size_t>(segOff + off);
                std::int64_t tag = static_cast<std::int64_t>(read64(elfBytes, pos));
                std::uint64_t val = read64(elfBytes, pos + 8);

                if (tag == DT_NULL) break;
                if (tag == DT_INIT || tag == DT_OS_INIT) addIfInText(val);
                if (tag == DT_FINI || tag == DT_OS_FINI) addIfInText(val);
                if (tag == DT_INIT_ARRAY || tag == DT_OS_INIT_ARRAY) initArrayVa = val;
                if (tag == DT_INIT_ARRAYSZ || tag == DT_OS_INIT_ARRAYSZ) initArraySz = val;
                if (tag == DT_FINI_ARRAY || tag == DT_OS_FINI_ARRAY) finiArrayVa = val;
                if (tag == DT_FINI_ARRAYSZ || tag == DT_OS_FINI_ARRAYSZ) finiArraySz = val;
            }

            auto collectArray = [&](VirtualAddress arrayVa, std::uint64_t arraySz) {
                if (arrayVa == 0 || arraySz == 0) return;
                std::uint64_t phFileOff = read64(elfBytes, phPos + 8);
                std::uint64_t phVaddr = read64(elfBytes, phPos + 16);
                if (arrayVa < phVaddr || !fitsAt(phFileOff, arrayVa - phVaddr, 0, elfBytes.size())) return;
                std::uint64_t arrayFileOff = phFileOff + (arrayVa - phVaddr);
                std::uint64_t count = arraySz / 8;
                for (std::uint64_t k = 0; k < count; ++k) {
                    if (!fitsAt(arrayFileOff, k * 8, 8, elfBytes.size())) break;
                    std::size_t entPos = static_cast<std::size_t>(arrayFileOff + k * 8);
                    addIfInText(read64(elfBytes, entPos));
                }
            };

            collectArray(initArrayVa, initArraySz);
            collectArray(finiArrayVa, finiArraySz);

            break;
        }

        std::uint64_t shOff = read64(elfBytes, 40);
        std::uint16_t shEntSize = read16(elfBytes, 58);
        std::uint16_t shCount = read16(elfBytes, 60);
        std::uint16_t shStrIdx = read16(elfBytes, 62);

        if (shEntSize >= 64 && shOff != 0 && shCount > 0 && shStrIdx < shCount) {
            const std::uint64_t shStrIndexOff = static_cast<std::uint64_t>(shStrIdx) * shEntSize;
            if (fitsAt(shOff, shStrIndexOff, 64, elfBytes.size())) {
                std::size_t shStrPos = static_cast<std::size_t>(shOff + shStrIndexOff);
                std::uint64_t strTabOff = read64(elfBytes, shStrPos + 24);
                std::uint64_t strTabSz = read64(elfBytes, shStrPos + 32);

                for (std::uint16_t si = 0; si < shCount; ++si) {
                    const std::uint64_t shIndexOff = static_cast<std::uint64_t>(si) * shEntSize;
                    if (!fitsAt(shOff, shIndexOff, 64, elfBytes.size())) break;
                    std::size_t shPos = static_cast<std::size_t>(shOff + shIndexOff);
                    std::uint32_t shType = read32(elfBytes, shPos + 4);

                    if (shType != SHT_DYNSYM && shType != SHT_SYMTAB) continue;

                    std::uint64_t symOff = read64(elfBytes, shPos + 24);
                    std::uint64_t symSz = read64(elfBytes, shPos + 32);
                    std::uint32_t symLink = read32(elfBytes, shPos + 40);
                    std::uint64_t entSz = read64(elfBytes, shPos + 56);
                    if (entSz == 0) entSz = 24;

                    std::uint64_t strOff = 0;
                    if (symLink < shCount) {
                        const std::uint64_t strShIndexOff = static_cast<std::uint64_t>(symLink) * shEntSize;
                        if (fitsAt(shOff, strShIndexOff, 64, elfBytes.size()))
                            strOff = read64(elfBytes, static_cast<std::size_t>(shOff + strShIndexOff) + 24);
                    } else {
                        strOff = strTabOff;
                        (void)strTabSz;
                    }
                    (void)strOff;

                    std::uint64_t symCount = symSz / entSz;
                    for (std::uint64_t k = 0; k < symCount; ++k) {
                        if (!fitsAt(symOff, k * entSz, 24, elfBytes.size())) break;
                        std::size_t sPos = static_cast<std::size_t>(symOff + k * entSz);
                        std::uint8_t info = elfBytes[sPos + 4];
                        std::uint8_t stBind = info >> 4;
                        std::uint8_t stType = info & 0xF;
                        std::uint16_t shndx = read16(elfBytes, sPos + 6);
                        std::uint64_t symVal = read64(elfBytes, sPos + 8);
                        if ((stBind == STB_GLOBAL || stBind == STB_WEAK) &&
                            stType == STT_FUNC && shndx != SHN_UNDEF) {
                            addIfInText(symVal);
                        }
                    }
                }
            }
        }

        if (entries.empty()) throw RelinkerException("No entry points found in text segment");
        return entries;
    }
};

std::unique_ptr<IEntryPointCollector> MakeEntryPointCollector() {
    return std::make_unique<EntryPointCollector>();
}

}
