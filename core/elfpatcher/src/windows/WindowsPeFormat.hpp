#ifndef CORE_ELFPATCHER_SRC_WINDOWS_WINDOWSPEFORMAT_HPP
#define CORE_ELFPATCHER_SRC_WINDOWS_WINDOWSPEFORMAT_HPP

constexpr std::uint16_t kMzMagic = 0x5A4D;
constexpr std::uint32_t kPeSignature = 0x00004550;
constexpr std::uint16_t kMachinAmd64 = 0x8664;
constexpr std::uint16_t kOptMagicPe32Plus = 0x020B;
constexpr std::uint16_t kCharsDll = 0x2000;
constexpr std::uint16_t kCharsExe = 0x0002;
constexpr std::uint16_t kCharsLargeAddressAware = 0x0020;
constexpr std::uint32_t kSectionAlignment = 0x1000;
constexpr std::uint32_t kFileAlignment = 0x200;
constexpr std::uint32_t kOsVersion = 6;
constexpr std::uint32_t kOsVersionMinor = 0;
constexpr std::uint32_t kSubsystemConsole = 3;
constexpr std::uint32_t kNumberOfRvaAndSizes = 16;
constexpr std::uint32_t kSizeOfPeHeaders = 0x400;

constexpr std::uint32_t kRelTypeDir64 = 10;
constexpr std::uint32_t kRelTypeAbsolute = 0;
constexpr std::uint32_t kRelocBlockHeaderSize = 8;
constexpr std::uint32_t kRelocEntrySize = 2;

constexpr std::uint32_t kSecExec = 0x60000020;
constexpr std::uint32_t kSecRW = 0xC0000040;
constexpr std::uint32_t kSecRO = 0x40000040;
constexpr std::uint32_t kSecDiscard = 0x02000000;

struct MzHeader {
    std::uint8_t data[0x40];
};

struct PeSection {
    char name[8];
    std::uint32_t virtualSize;
    std::uint32_t virtualAddress;
    std::uint32_t rawSize;
    std::uint32_t rawOffset;
    std::uint32_t characteristics;
    std::vector<std::uint8_t> data;
};

struct DataDirectory {
    std::uint32_t rva;
    std::uint32_t size;
};

struct LoadSegment {
    std::uint64_t vaddr;
    std::uint64_t memSize;
    std::uint64_t fileOffset;
    std::uint64_t fileSize;
    std::uint32_t flags;
};

struct ImportThunkEntry {
    std::string symbolName;
    std::uint32_t gotRva;
};

struct ImportDllBlock {
    std::string dllName;
    std::vector<ImportThunkEntry> thunks;
};

#endif