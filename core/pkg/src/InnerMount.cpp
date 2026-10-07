#include <pkg/InnerMount.hpp>
#include <pkg/KrakenStream.hpp>
#include <pkg/PackageError.hpp>
#include <algorithm>
#include <cstring>

namespace Pkg {

InnerMount::InnerMount(const OuterImage& outer, const OuterFile& source, InnerLayout layout, std::filesystem::path oodlePath, const std::size_t cachedBlocks)
    : _outer(outer), _source(source), _layout(std::move(layout)), _oodlePath(std::move(oodlePath)), _cachedBlocks(std::max<std::size_t>(cachedBlocks, 1)) {}

std::uint64_t InnerMount::Size() const {
    return _layout.MountSize;
}

const std::vector<std::uint64_t>& InnerMount::FileOffsets() const {
    return _layout.FileOffsets;
}

void InnerMount::Read(std::uint64_t offset, std::uint8_t* destination, std::size_t size) {
    if (offset > _layout.MountSize || size > _layout.MountSize - offset) throw PackageError("Read outside the inner package mount", offset);
    const auto& blocks = _layout.Blocks;
    auto found = std::upper_bound(blocks.begin(), blocks.end(), offset, [](const std::uint64_t value, const InnerBlock& block) { return value < block.MountOffset; });
    while (size > 0) {
        if (found == blocks.begin()) throw PackageError("Inner package mount has no block", offset);
        const auto& block = *(found - 1);
        const std::uint64_t within = offset - block.MountOffset;
        if (within >= block.Length) throw PackageError("Inner package mount has a gap", offset);
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(size, block.Length - within));
        if (block.Kind == InnerBlockKind::Zero) std::memset(destination, 0, count);
        else if (block.Kind == InnerBlockKind::Stored) _outer.Read(_source, block.SourceOffset + within, destination, count);
        else std::memcpy(destination, decodeKraken(static_cast<std::size_t>(found - 1 - blocks.begin()))->data() + within, count);
        offset += count;
        destination += count;
        size -= count;
        ++found;
    }
}

InnerMount::Block InnerMount::decodeKraken(const std::size_t index) {
    {
        std::lock_guard lock(_cacheMutex);
        const auto cached = _cache.find(index);
        if (cached != _cache.end()) {
            _recent.splice(_recent.begin(), _recent, cached->second.second);
            return cached->second.first;
        }
    }
    const auto& block = _layout.Blocks[index];
    std::vector<std::uint8_t> payload(block.SourceLength);
    _outer.Read(_source, block.SourceOffset, payload.data(), payload.size());
    const auto stream = BuildKrakenStream(payload.data(), payload.size(), block.Length, block.EvenSourceLength, block.KrakenFlags);
    auto decoded = std::make_shared<std::vector<std::uint8_t>>(block.Length);
    oodle().Decompress(stream, decoded->data(), decoded->size());

    std::lock_guard lock(_cacheMutex);
    const auto cached = _cache.find(index);
    if (cached != _cache.end()) return cached->second.first;
    _recent.push_front(index);
    _cache.emplace(index, std::pair{Block(decoded), _recent.begin()});
    while (_cache.size() > _cachedBlocks) {
        _cache.erase(_recent.back());
        _recent.pop_back();
    }
    return decoded;
}

const OodleLibrary& InnerMount::oodle() {
    std::lock_guard lock(_oodleMutex);
    if (!_oodle) {
        if (_oodlePath.empty()) throw PackageError("The package contains Kraken-compressed data; an Oodle library (oo2core) path is required");
        _oodle = std::make_unique<OodleLibrary>(_oodlePath);
    }
    return *_oodle;
}

}
