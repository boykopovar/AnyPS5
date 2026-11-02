#ifndef PS5LIFT_GUESTIMAGE_HPP
#define PS5LIFT_GUESTIMAGE_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace Ps5Lift {

struct GuestTls {
    std::uint64_t Address = 0;
    std::uint64_t FileSize = 0;
    std::uint64_t MemorySize = 0;
    std::uint64_t Alignment = 1;
};

struct GuestTlsPlacement {
    std::uint64_t Module = 0;
    std::uint64_t Offset = 0;
};

struct GuestDataImport {
    std::uint64_t Slot;
    std::string Name;
    std::int64_t Addend;
};

struct GuestSegment {
    std::uint64_t Address;
    std::uint64_t MemorySize;
    std::vector<std::uint8_t> Bytes;
    std::uint32_t Flags;

    bool IsExecutable() const;
    bool IsWritable() const;
};

struct GuestArray {
    std::uint64_t Address = 0;
    std::uint64_t Count = 0;
};

struct GuestRelocation {
    std::uint64_t Offset;
    std::uint32_t Type;
    std::int64_t Addend;
    std::string Symbol;
    bool Defined;
    bool Object;
    std::uint64_t Value;
};

class GuestImageFile {
public:
    static GuestImageFile Read(const std::string& path, std::optional<std::uint64_t> base);

    const std::string& Path() const;
    const std::vector<std::string>& Needed() const;
    std::uint64_t End() const;
    const GuestTls& Tls() const;
    std::optional<std::uint64_t> Export(const std::string& name) const;
    std::set<std::string> UndefinedSymbols() const;

private:
    friend class GuestImage;

    std::string _path;
    std::vector<std::string> _needed;
    std::map<std::string, std::uint64_t> _exports;
    std::set<std::string> _indirectFunctions;
    std::set<std::uint64_t> _functionExports;
    std::vector<GuestRelocation> _relocations;
    std::uint64_t _mappedEnd = 0;
    std::uint64_t _loadBias = 0;
    GuestArray _programHeaders;
    std::uint64_t _init = 0;
    GuestArray _initArray;
    std::uint64_t _entry = 0;
    std::vector<GuestSegment> _segments;
    std::map<std::uint64_t, std::string> _functionSymbols;
    std::set<std::uint64_t> _exceptionStarts;
    std::map<std::uint64_t, std::set<std::uint64_t>> _landingPads;
    GuestTls _tls;
};

class GuestImage {
public:
    GuestImage(GuestImageFile file, GuestTlsPlacement tlsPlacement, const std::map<std::string, std::uint64_t>& definitions);

    const std::string& Path() const;
    std::uint64_t LoadBias() const;
    const GuestArray& ProgramHeaders() const;
    const std::map<std::string, std::uint64_t>& Exports() const;
    const std::set<std::uint64_t>& FunctionExports() const;
    std::uint64_t Init() const;
    const GuestArray& InitArray() const;
    std::uint64_t Entry() const;
    const std::vector<GuestSegment>& Segments() const;
    std::span<const std::uint8_t> CodeAt(std::uint64_t address) const;
    std::optional<std::uint64_t> ReadData(std::uint64_t address, unsigned size) const;
    bool IsExecutable(std::uint64_t address) const;
    bool IsReadOnly(std::uint64_t address, std::uint64_t size) const;
    bool Relocates(std::uint64_t address, std::uint64_t size) const;
    const std::map<std::uint64_t, std::string>& FunctionSymbols() const;
    const std::map<std::uint64_t, std::string>& Imports() const;
    std::optional<std::uint64_t> ImportAt(std::uint64_t slot) const;
    const std::set<std::uint64_t>& CodePointers() const;
    const std::set<std::uint64_t>& ExceptionFunctionStarts() const;
    const std::map<std::uint64_t, std::set<std::uint64_t>>& LandingPads() const;
    const std::vector<GuestDataImport>& DataImports() const;
    const GuestTls& Tls() const;
    const GuestTlsPlacement& TlsPlacement() const;

private:
    std::string _path;
    std::uint64_t _loadBias;
    GuestArray _programHeaders;
    std::map<std::string, std::uint64_t> _exports;
    std::set<std::uint64_t> _functionExports;
    std::uint64_t _init;
    GuestArray _initArray;
    std::uint64_t _entry;
    std::vector<GuestSegment> _segments;
    std::map<std::uint64_t, std::string> _functionSymbols;
    std::set<std::uint64_t> _exceptionStarts;
    std::map<std::uint64_t, std::set<std::uint64_t>> _landingPads;
    GuestTls _tls;
    GuestTlsPlacement _tlsPlacement;
    std::vector<std::uint64_t> _relocatedAddresses;
    std::map<std::uint64_t, std::string> _imports;
    std::map<std::uint64_t, std::uint64_t> _importSlots;
    std::set<std::uint64_t> _codePointers;
    std::vector<GuestDataImport> _dataImports;

    void Write64(std::uint64_t address, std::uint64_t value);
};

}

#endif
