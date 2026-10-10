#include <Testing/Test.hpp>
#include <nid/BinaryPatcherFactory.hpp>
#include <nid/ElfPatcher.hpp>
#include <nid/PeNidPatcher.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

using Testing::Case;
using Testing::Require;

const Case elfMagic{"MakePatcher_ElfMagic_ReturnsElfPatcher", [] {
    const auto patcher = Nid::MakePatcher({0x7f, 'E', 'L', 'F', 2, 1});
    Require(dynamic_cast<const Nid::ElfNidPatcher*>(patcher.get()) != nullptr, "ELF patcher selected");
}};

const Case peMagic{"MakePatcher_MzMagic_ReturnsPePatcher", [] {
    const auto patcher = Nid::MakePatcher({'M', 'Z', 0x90, 0});
    Require(dynamic_cast<const Nid::PeNidPatcher*>(patcher.get()) != nullptr, "PE patcher selected");
}};

const Case unknownFormat{"MakePatcher_UnknownMagic_Throws", [] {
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [] { Nid::MakePatcher({'#', '!', '/', 'b'}); }, "unrecognized binary format", "unknown magic");
}};

const Case truncatedMagic{"MakePatcher_TruncatedElfMagic_Throws", [] {
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [] { Nid::MakePatcher({0x7f, 'E', 'L'}); }, "unrecognized binary format", "three-byte ELF magic");
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [] { Nid::MakePatcher({'M'}); }, "unrecognized binary format", "one-byte MZ magic");
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [] { Nid::MakePatcher({}); }, "unrecognized binary format", "empty input");
}};

} // namespace
