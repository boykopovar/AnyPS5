#include "prx/libSceMetadataReaderWriter/libSceMetadataReaderWriter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libSceMetadataReaderWriter check failed at line %u\n", location.line());
    std::abort();
}

template <typename Callable>
bool ThrowsInvalidArgument(Callable&& callable) {
    try {
        callable();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct Fields {
    std::uint32_t reserved;
    std::uint32_t count;
};

struct Value {
    std::uint32_t type;
    std::uint32_t padding;
    std::uint64_t word;
    std::uint64_t payload;
};

static_assert(sizeof(Fields) == 0x8);
static_assert(sizeof(Value) == 0x18);

} // namespace

int main() {
    std::array<std::byte, 0x40> metadata;
    metadata.fill(std::byte{0x5A});
    const std::uint64_t noFields = 0;
    std::memcpy(metadata.data() + 0x28, &noFields, sizeof(noFields));
    Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(metadata.data()) == 0);

    Fields fields{0x11223344u, 7u};
    std::uint64_t pointer = reinterpret_cast<std::uint64_t>(&fields);
    std::memcpy(metadata.data() + 0x28, &pointer, sizeof(pointer));
    Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(metadata.data()) == 7);
    fields.count = 0;
    Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(metadata.data()) == 0);
    Require(ThrowsInvalidArgument([] { _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(nullptr); }));

    std::array<std::byte, 0x38> source;
    for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<std::byte>(index + 1u);
    std::array<std::byte, 0x38> destination;
    destination.fill(std::byte{0xA5});
    _ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_(destination.data(), source.data());
    Require(destination == source);
    destination.fill(std::byte{0xA5});
    _ZN23sceMetadataReaderWriter15ParserInterfaceC2ERKS0_(destination.data(), source.data());
    Require(destination == source);
    _ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_(source.data(), source.data());
    Require(source[0] == std::byte{1});
    Require(ThrowsInvalidArgument([] { _ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_(nullptr, nullptr); }));

    Value original{2u, 0xDEADBEEFu, 0x1122334455667788ull, 0x99AABBCCDDEEFF00ull};
    Value assigned{9u, 0xAAAA5555u, 0xBBBBCCCCDDDDEEEEull, 0x0123456789ABCDEFull};
    Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&assigned, &original) == &assigned);
    Require(assigned.type == original.type);
    Require(assigned.padding == 0xAAAA5555u);
    Require(assigned.word == original.word);
    Require(assigned.payload == original.payload);

    Value self{3u, 1u, 2u, 3u};
    Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&self, &self) == &self);
    Require(self.type == 3u && self.padding == 1u && self.word == 2u && self.payload == 3u);

    Value numeric{1u, 0x42u, 0x1234567890ABCDEFull, 0xCAFEBABEDEADBEEFull};
    Value target{};
    Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&target, &numeric) == &target);
    Require(target.type == numeric.type && target.padding == 0u && target.word == numeric.word && target.payload == numeric.payload);

    Value invalid{4u, 0x11u, 0x22u, 0x33u};
    Value untouched{0u, 0u, 0u, 0xAAAAAAAA55555555ull};
    Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&untouched, &invalid) == &untouched);
    Require(untouched.type == 4u && untouched.padding == 0u && untouched.word == 0x22u && untouched.payload == 0xAAAAAAAA55555555ull);
    Require(ThrowsInvalidArgument([] { _ZN23sceMetadataReaderWriter5ValueaSERS0_(nullptr, nullptr); }));
    return 0;
}
