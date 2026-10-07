#ifndef PKG_INNERMOUNT_HPP
#define PKG_INNERMOUNT_HPP

#include <pkg/NapsLayout.hpp>
#include <pkg/OodleLibrary.hpp>
#include <pkg/OuterImage.hpp>
#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Pkg {

class InnerMount {
public:
    InnerMount(const OuterImage& outer, const OuterFile& source, InnerLayout layout, std::filesystem::path oodlePath, std::size_t cachedBlocks = 64);

    std::uint64_t Size() const;
    const std::vector<std::uint64_t>& FileOffsets() const;
    void Read(std::uint64_t offset, std::uint8_t* destination, std::size_t size);

private:
    using Block = std::shared_ptr<const std::vector<std::uint8_t>>;

    Block decodeKraken(std::size_t index);
    const OodleLibrary& oodle();

    const OuterImage& _outer;
    const OuterFile& _source;
    const InnerLayout _layout;
    const std::filesystem::path _oodlePath;
    const std::size_t _cachedBlocks;
    std::mutex _oodleMutex;
    std::unique_ptr<OodleLibrary> _oodle;
    std::mutex _cacheMutex;
    std::list<std::size_t> _recent;
    std::unordered_map<std::size_t, std::pair<Block, std::list<std::size_t>::iterator>> _cache;
};

}

#endif
