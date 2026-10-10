#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"

#include <cstdint>
#include <map>
#include <optional>

namespace {

using namespace AgcDriver::Graphics;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct Keys {
    std::map<std::uint64_t, DccKeys> ranges;
    int reads = 0;

    DccKeys Read(std::uint64_t address) {
        ++reads;
        if (address == 0) return DccKeys::Uncompressed;
        const auto it = ranges.find(address);
        return it == ranges.end() ? DccKeys::Uncompressed : it->second;
    }
};

struct Image {
    std::uint64_t dcc = 0;
    DccKeys uploaded = DccKeys::Uncompressed;
    DccKeys filled = DccKeys::Uncompressed;
};

struct SurfaceCache {
    Keys& keys;
    bool shared = true;
    std::optional<Image> image;
    int made = 0;
    int remade = 0;
    int uploads = 0;

    bool Serves(const Image& held, std::uint64_t named) {
        if (!shared) return named == 0 || named == held.dcc;
        return KeysServeSurface(held.dcc, held.uploaded, held.filled, named, [&] { return keys.Read(held.dcc); }, [&] { return keys.Read(named); });
    }

    const Image& Lookup(std::uint64_t named) {
        if (image.has_value() && Serves(*image, named)) {
            const auto current = keys.Read(image->dcc);
            if (current != image->uploaded) {
                image->uploaded = current;
                ++uploads;
            }
            return *image;
        }
        if (image.has_value()) ++remade;
        else ++made;
        image = Image{named, keys.Read(named), DccKeys::Uncompressed};
        ++uploads;
        return *image;
    }
};

constexpr std::uint64_t TargetKeys = 0x570526000;
constexpr std::uint64_t StorageKeys = 0x5705e0000;

void RenderWorldMapFrames(SurfaceCache& cache) {
    for (int frame = 0; frame < 100; ++frame) {
        cache.Lookup(TargetKeys);
        cache.Lookup(StorageKeys);
        cache.Lookup(StorageKeys);
    }
}

const Case sharedWorldMap{"KeysServeSurface_TwoDescriptorsOverUncompressedKeys_ShareOneImage", [] {
    Keys keys;
    SurfaceCache cache{keys, true};
    RenderWorldMapFrames(cache);
    Require(cache.made == 1 && cache.remade == 0, "a surface whose two descriptors name uncompressed keys was remade");
    RequireEqual(cache.uploads, 1, "a shared surface was uploaded again with nothing changed");
    Require(cache.image->dcc == TargetKeys, "the shared image left the keys it follows");
}};

const Case unsharedWorldMap{"SurfaceCache_WithoutKeySharing_RemakesTheImageTwiceAFrame", [] {
    Keys keys;
    SurfaceCache cache{keys, false};
    RenderWorldMapFrames(cache);
    RequireEqual(cache.remade, 199, "the unshared model does not reproduce the map's two remakes a frame");
}};

const Case namedClear{"KeysServeSurface_FastClearOfNamedKeys_RemakesTheImage", [] {
    Keys keys;
    SurfaceCache cache{keys};
    cache.Lookup(TargetKeys);
    keys.ranges[StorageKeys] = DccKeys::Clear0000;
    const auto& image = cache.Lookup(StorageKeys);
    Require(cache.remade == 1 && image.dcc == StorageKeys && image.uploaded == DccKeys::Clear0000, "a fast clear of the named keys did not reach the surface's image");
    const auto& own = cache.Lookup(StorageKeys);
    Require(cache.remade == 1 && own.uploaded == DccKeys::Clear0000, "the image of the cleared keys did not serve its own keys");
    cache.Lookup(TargetKeys);
    RequireEqual(cache.remade, 2, "uncompressed keys shared an image holding a clear");
}};

const Case followedClear{"KeysServeSurface_FastClearOfFollowedKeys_RemakesTheImage", [] {
    Keys keys;
    SurfaceCache cache{keys};
    cache.Lookup(TargetKeys);
    keys.ranges[TargetKeys] = DccKeys::Clear1111;
    const auto& image = cache.Lookup(StorageKeys);
    Require(cache.remade == 1 && image.dcc == StorageKeys && image.uploaded == DccKeys::Uncompressed, "a descriptor over uncompressed keys was served an image whose own keys are a clear");
}};

const Case filledKeys{"KeysServeSurface_ImageHoldingKeyFill_DoesNotServeOtherKeys", [] {
    Keys keys;
    SurfaceCache cache{keys};
    cache.Lookup(TargetKeys);
    cache.image->filled = DccKeys::Clear0001;
    cache.Lookup(StorageKeys);
    RequireEqual(cache.remade, 1, "an image holding a key fill served other keys");
}};

const Case ownOrNoKeys{"KeysServeSurface_OwnKeysOrNoMetadata_ServesWithoutScanning", [] {
    Keys keys;
    keys.ranges[TargetKeys] = DccKeys::Clear0000;
    Require(KeysServeSurface(TargetKeys, DccKeys::Clear0000, DccKeys::Uncompressed, 0, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(0); }), "a descriptor without metadata was refused");
    Require(KeysServeSurface(TargetKeys, DccKeys::Clear0000, DccKeys::Uncompressed, TargetKeys, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(TargetKeys); }), "the image's own keys were refused");
    RequireEqual(keys.reads, 0, "the own-keys and no-metadata answers scanned keys");
}};

const Case uploadedClear{"KeysServeSurface_ImageUploadedUnderClear_RefusesOtherKeysWithoutScanning", [] {
    Keys keys;
    keys.ranges[TargetKeys] = DccKeys::Clear0000;
    Require(!KeysServeSurface(TargetKeys, DccKeys::Clear0000, DccKeys::Uncompressed, StorageKeys, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(StorageKeys); }), "an image uploaded under a clear served other keys");
    RequireEqual(keys.reads, 0, "a refusal by the uploaded keys scanned keys");
}};

const Case compressedKeys{"KeysServeSurface_NamedKeysNotUncompressed_AreRefused", [] {
    Keys keys;
    keys.ranges[StorageKeys] = DccKeys::Mixed;
    keys.ranges[TargetKeys] = DccKeys::Uncompressed;
    Require(!KeysServeSurface(TargetKeys, DccKeys::Uncompressed, DccKeys::Uncompressed, StorageKeys, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(StorageKeys); }), "keys that do not read uncompressed were served");
}};

const Case imageWithoutMetadata{"KeysServeSurface_ImageWithoutMetadata_ServesUncompressedKeysWithoutReadingItsOwn", [] {
    Keys keys;
    int followedReads = 0;
    Require(KeysServeSurface(0, DccKeys::Uncompressed, DccKeys::Uncompressed, TargetKeys, [&] { ++followedReads; return DccKeys::Clear0000; }, [&] { return keys.Read(TargetKeys); }), "an image without metadata did not serve uncompressed keys");
    RequireEqual(followedReads, 0, "an image without metadata scanned keys of its own");
}};

} // namespace
