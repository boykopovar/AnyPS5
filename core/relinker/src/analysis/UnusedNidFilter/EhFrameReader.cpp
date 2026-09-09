#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <cstring>
#include <limits>

namespace Relinker::UnusedNidFilter {

namespace {

struct Cie {
    std::uint8_t PointerEncoding = 0;
    std::uint8_t LsdaEncoding = 0xFF;
    bool Augmented = false;
    VirtualAddress Personality = 0;
};

class Reader {
public:
    Reader(const std::vector<std::uint8_t>& bytes, const std::vector<ProgramHeader>& headers, const std::map<VirtualAddress, VirtualAddress>& pointers, const std::set<VirtualAddress>& importSlots) : bytes(bytes), headers(headers), pointers(pointers), importSlots(importSlots) {}

    std::vector<StrictCodeRegion> Read() {
        std::vector<StrictCodeRegion> result;
        for (const auto& segment : headers) {
            if (segment.Type != 0x6474E550 || segment.FileSize == 0) continue;
            auto position = segment.MappedAddress;
            const auto version = read<std::uint8_t>(position);
            const auto frameEncoding = read<std::uint8_t>(position);
            const auto countEncoding = read<std::uint8_t>(position);
            const auto tableEncoding = read<std::uint8_t>(position);
            if (version != 1 || frameEncoding == 0xFF || countEncoding == 0xFF || tableEncoding == 0xFF)
                throw RelinkerException("Strict filter: unsupported EH frame header", segment.MappedAddress);
            encoded(position, frameEncoding, segment.MappedAddress);
            const auto count = encoded(position, countEncoding, segment.MappedAddress);
            if (count > segment.FileSize / 2) throw RelinkerException("Strict filter: invalid EH frame count", position);
            for (std::uint64_t index = 0; index < count; ++index) {
                const auto begin = encoded(position, tableEncoding, segment.MappedAddress);
                const auto fde = encoded(position, tableEncoding, segment.MappedAddress);
                if (position > segment.MappedAddress + segment.FileSize) throw RelinkerException("Strict filter: EH frame table exceeds segment", position);
                result.push_back(readFde(begin, fde));
            }
        }
        return result;
    }

private:
    const std::vector<std::uint8_t>& bytes;
    const std::vector<ProgramHeader>& headers;
    const std::map<VirtualAddress, VirtualAddress>& pointers;
    const std::set<VirtualAddress>& importSlots;
    std::map<VirtualAddress, Cie> cies;

    template<typename TValue>
    TValue read(VirtualAddress& address) const {
        for (const auto& segment : headers) {
            if (segment.Type != 1 || address < segment.MappedAddress || address - segment.MappedAddress > segment.FileSize || sizeof(TValue) > segment.FileSize - (address - segment.MappedAddress)) continue;
            const auto offset = segment.Offset + address - segment.MappedAddress;
            if (offset > bytes.size() || sizeof(TValue) > bytes.size() - offset) throw RelinkerException("Strict filter: unwind data exceeds file", address);
            TValue value;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            address += sizeof(value);
            return value;
        }
        throw RelinkerException("Strict filter: unwind address is not file-backed", address);
    }

    std::uint64_t leb(VirtualAddress& position, bool signedValue = false) const {
        std::uint64_t value = 0;
        unsigned shift = 0;
        for (;;) {
            const auto byte = read<std::uint8_t>(position);
            if (shift == 63 && ((byte & 0x7F) != 0 && (byte & 0x7F) != 1 && (!signedValue || (byte & 0x7F) != 0x7F)))
                throw RelinkerException("Strict filter: overflowing LEB128 value", position);
            value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
            shift += 7;
            if ((byte & 0x80) == 0) {
                if (signedValue && (byte & 0x40) != 0 && shift < 64) value |= (~std::uint64_t{0}) << shift;
                return value;
            }
            if (shift >= 64) throw RelinkerException("Strict filter: unterminated LEB128 value", position);
        }
    }

    std::uint64_t encoded(VirtualAddress& position, std::uint8_t encoding, VirtualAddress dataBase = 0, VirtualAddress functionBase = 0) const {
        const auto field = position;
        std::uint64_t value;
        switch (encoding & 0x0F) {
            case 0: value = read<std::uint64_t>(position); break;
            case 1: value = leb(position); break;
            case 2: value = read<std::uint16_t>(position); break;
            case 3: value = read<std::uint32_t>(position); break;
            case 4: value = read<std::uint64_t>(position); break;
            case 9: value = leb(position, true); break;
            case 10: value = static_cast<std::uint64_t>(static_cast<std::int64_t>(read<std::int16_t>(position))); break;
            case 11: value = static_cast<std::uint64_t>(static_cast<std::int64_t>(read<std::int32_t>(position))); break;
            case 12: value = static_cast<std::uint64_t>(read<std::int64_t>(position)); break;
            default: throw RelinkerException("Strict filter: unsupported unwind pointer format", field);
        }
        if (value == 0) return 0;
        switch (encoding & 0x70) {
            case 0: break;
            case 0x10: value += field; break;
            case 0x30: value += dataBase; break;
            case 0x40: value += functionBase; break;
            default: throw RelinkerException("Strict filter: unsupported unwind pointer base", field);
        }
        if ((encoding & 0x80) != 0 && !importSlots.contains(value)) {
            const auto pointer = pointers.find(value);
            if (pointer != pointers.end()) value = pointer->second;
            else value = read<std::uint64_t>(value);
        }
        return value;
    }

    Cie readCie(VirtualAddress address) {
        if (const auto found = cies.find(address); found != cies.end()) return found->second;
        auto position = address;
        const auto length = read<std::uint32_t>(position);
        if (length == 0xFFFFFFFF || length < 8) throw RelinkerException("Strict filter: unsupported CIE record size", address);
        const auto end = position + length;
        if (read<std::uint32_t>(position) != 0) throw RelinkerException("Strict filter: invalid CIE identifier", address);
        const auto version = read<std::uint8_t>(position);
        if (version != 1 && version != 3 && version != 4) throw RelinkerException("Strict filter: unsupported CIE version", address);
        std::string augmentation;
        for (;;) {
            if (position >= end) throw RelinkerException("Strict filter: unterminated CIE augmentation", address);
            const auto character = read<std::uint8_t>(position);
            if (character == 0) break;
            augmentation.push_back(static_cast<char>(character));
        }
        if (version == 4 && (read<std::uint8_t>(position) != 8 || read<std::uint8_t>(position) != 0))
            throw RelinkerException("Strict filter: unsupported CIE address size", address);
        leb(position);
        leb(position, true);
        if (version == 1) read<std::uint8_t>(position);
        else leb(position);
        Cie cie;
        if (!augmentation.empty()) {
            if (augmentation.front() != 'z') throw RelinkerException("Strict filter: unsupported CIE augmentation", address);
            cie.Augmented = true;
            const auto size = leb(position);
            const auto augmentationEnd = position + size;
            if (augmentationEnd > end) throw RelinkerException("Strict filter: CIE augmentation exceeds record", address);
            for (std::size_t index = 1; index < augmentation.size(); ++index) {
                switch (augmentation[index]) {
                    case 'L': cie.LsdaEncoding = read<std::uint8_t>(position); break;
                    case 'R': cie.PointerEncoding = read<std::uint8_t>(position); break;
                    case 'P': {
                        const auto encoding = read<std::uint8_t>(position);
                        cie.Personality = encoded(position, encoding);
                        break;
                    }
                    case 'S': break;
                    default: throw RelinkerException("Strict filter: unknown CIE augmentation field", position);
                }
            }
            if (position != augmentationEnd) throw RelinkerException("Strict filter: CIE augmentation size mismatch", address);
        }
        if (position > end) throw RelinkerException("Strict filter: CIE fields exceed record", address);
        cies.emplace(address, cie);
        return cie;
    }

    StrictCodeRegion readFde(VirtualAddress expectedBegin, VirtualAddress address) {
        auto position = address;
        const auto length = read<std::uint32_t>(position);
        if (length == 0xFFFFFFFF || length < 8) throw RelinkerException("Strict filter: unsupported FDE record size", address);
        const auto end = position + length;
        const auto cieField = position;
        const auto cieOffset = read<std::uint32_t>(position);
        if (cieOffset > cieField) throw RelinkerException("Strict filter: invalid FDE CIE offset", address);
        const auto cie = readCie(cieField - cieOffset);
        const auto begin = encoded(position, cie.PointerEncoding);
        const auto size = encoded(position, cie.PointerEncoding & 0x0F);
        if (begin != expectedBegin || size == 0 || size > std::numeric_limits<VirtualAddress>::max() - begin)
            throw RelinkerException("Strict filter: inconsistent FDE function range", address);
        StrictCodeRegion region{begin, begin + size, {}};
        if (cie.Personality != 0) region.ExtraTargets.push_back(cie.Personality);
        if (cie.Augmented) {
            const auto augmentationSize = leb(position);
            const auto augmentationEnd = position + augmentationSize;
            if (augmentationEnd > end) throw RelinkerException("Strict filter: FDE augmentation exceeds record", address);
            if (cie.LsdaEncoding != 0xFF) {
                const auto lsda = encoded(position, cie.LsdaEncoding, 0, begin);
                if (lsda != 0) readLsda(lsda, region);
            }
            if (position > augmentationEnd) throw RelinkerException("Strict filter: FDE augmentation size mismatch", address);
        }
        if (position > end) throw RelinkerException("Strict filter: FDE fields exceed record", address);
        return region;
    }

    void readLsda(VirtualAddress address, StrictCodeRegion& region) const {
        auto position = address;
        const auto landingEncoding = read<std::uint8_t>(position);
        const auto landingBase = landingEncoding == 0xFF ? region.Begin : encoded(position, landingEncoding, 0, region.Begin);
        const auto typeEncoding = read<std::uint8_t>(position);
        if (typeEncoding != 0xFF) leb(position);
        const auto callEncoding = read<std::uint8_t>(position);
        if ((callEncoding & 0xF0) != 0) throw RelinkerException("Strict filter: unsupported LSDA call-site encoding", address);
        const auto tableSize = leb(position);
        if (tableSize > bytes.size()) throw RelinkerException("Strict filter: invalid LSDA table size", address);
        const auto tableEnd = position + tableSize;
        while (position < tableEnd) {
            encoded(position, callEncoding);
            encoded(position, callEncoding);
            const auto landing = encoded(position, callEncoding);
            leb(position);
            if (landing != 0) region.ExtraTargets.push_back(landingBase + landing);
        }
        if (position != tableEnd) throw RelinkerException("Strict filter: LSDA call-site table size mismatch", address);
    }
};

}

std::vector<StrictCodeRegion> ReadExceptionFunctions(const std::vector<std::uint8_t>& bytes, const std::vector<ProgramHeader>& headers, const std::map<VirtualAddress, VirtualAddress>& pointers, const std::set<VirtualAddress>& importSlots) {
    return Reader(bytes, headers, pointers, importSlots).Read();
}

}
