#include <relinker/analysis/UnusedNidFilter/IEntryPointCollector.hpp>
#include <cstring>

namespace Relinker::UnusedNidFilter {

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
            std::size_t phPos = static_cast<std::size_t>(phOff) + i * phEntSize;
            if (phPos + 56 > elfBytes.size()) throw RelinkerException("Program header out of bounds");

            std::uint32_t type = read32(elfBytes, phPos);

            constexpr std::uint32_t PT_DYNAMIC = 2;
            if (type != PT_DYNAMIC) continue;

            std::uint64_t segOff = read64(elfBytes, phPos + 8);
            std::uint64_t segSz = read64(elfBytes, phPos + 32);

            if (segOff + segSz > elfBytes.size()) throw RelinkerException("PT_DYNAMIC segment out of bounds");

            constexpr std::int64_t DT_NULL = 0;
            constexpr std::int64_t DT_INIT = 12;
            constexpr std::int64_t DT_FINI = 13;
            constexpr std::int64_t DT_INIT_ARRAY = 25;
            constexpr std::int64_t DT_INIT_ARRAYSZ = 27;
            constexpr std::int64_t DT_FINI_ARRAY = 26;
            constexpr std::int64_t DT_FINI_ARRAYSZ = 28;
            constexpr std::int64_t DT_OS_INIT_ARRAY = 0x60000019;
            constexpr std::int64_t DT_OS_INIT_ARRAYSZ = 0x6000001b;
            constexpr std::int64_t DT_OS_FINI_ARRAY = 0x6000001a;
            constexpr std::int64_t DT_OS_FINI_ARRAYSZ = 0x6000001c;
            constexpr std::int64_t DT_OS_INIT = 0x6000000c;
            constexpr std::int64_t DT_OS_FINI = 0x6000000d;

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
                if (arrayVa < phVaddr) return;
                std::uint64_t arrayFileOff = phFileOff + (arrayVa - phVaddr);
                std::uint64_t count = arraySz / 8;
                for (std::uint64_t k = 0; k < count; ++k) {
                    std::size_t entPos = static_cast<std::size_t>(arrayFileOff + k * 8);
                    if (entPos + 8 > elfBytes.size()) break;
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
            std::size_t shStrPos = static_cast<std::size_t>(shOff) + shStrIdx * shEntSize;
            if (shStrPos + 64 <= elfBytes.size()) {
                std::uint64_t strTabOff = read64(elfBytes, shStrPos + 24);
                std::uint64_t strTabSz = read64(elfBytes, shStrPos + 32);

                for (std::uint16_t si = 0; si < shCount; ++si) {
                    std::size_t shPos = static_cast<std::size_t>(shOff) + si * shEntSize;
                    if (shPos + 64 > elfBytes.size()) break;
                    std::uint32_t shType = read32(elfBytes, shPos + 4);

                    constexpr std::uint32_t SHT_DYNSYM = 11;
                    constexpr std::uint32_t SHT_SYMTAB = 2;
                    if (shType != SHT_DYNSYM && shType != SHT_SYMTAB) continue;

                    std::uint64_t symOff = read64(elfBytes, shPos + 24);
                    std::uint64_t symSz = read64(elfBytes, shPos + 32);
                    std::uint32_t symLink = read32(elfBytes, shPos + 40);
                    std::uint64_t entSz = read64(elfBytes, shPos + 56);
                    if (entSz == 0) entSz = 24;

                    std::uint64_t strOff = 0;
                    if (symLink < shCount) {
                        std::size_t strShPos = static_cast<std::size_t>(shOff) + symLink * shEntSize;
                        if (strShPos + 64 <= elfBytes.size())
                            strOff = read64(elfBytes, strShPos + 24);
                    } else {
                        strOff = strTabOff;
                        (void)strTabSz;
                    }
                    (void)strOff;

                    std::uint64_t symCount = symSz / entSz;
                    for (std::uint64_t k = 0; k < symCount; ++k) {
                        std::size_t sPos = static_cast<std::size_t>(symOff + k * entSz);
                        if (sPos + 24 > elfBytes.size()) break;
                        std::uint8_t info = elfBytes[sPos + 4];
                        std::uint8_t stBind = info >> 4;
                        std::uint8_t stType = info & 0xF;
                        std::uint16_t shndx = read16(elfBytes, sPos + 6);
                        std::uint64_t symVal = read64(elfBytes, sPos + 8);
                        constexpr std::uint8_t STB_GLOBAL = 1;
                        constexpr std::uint8_t STB_WEAK = 2;
                        constexpr std::uint8_t STT_FUNC = 2;
                        constexpr std::uint16_t SHN_UNDEF = 0;
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
