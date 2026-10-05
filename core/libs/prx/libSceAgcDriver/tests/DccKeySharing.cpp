#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>

using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

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

void worldMapFrames() {
    for (const bool shared : {true, false}) {
        Keys keys;
        SurfaceCache cache{keys, shared};
        for (int frame = 0; frame < 100; ++frame) {
            cache.Lookup(TargetKeys);
            cache.Lookup(StorageKeys);
            cache.Lookup(StorageKeys);
        }
        if (shared) {
            Expect(cache.made == 1 && cache.remade == 0, "a surface whose two descriptors name uncompressed keys was remade");
            Expect(cache.uploads == 1, "a shared surface was uploaded again with nothing changed");
            Expect(cache.image->dcc == TargetKeys, "the shared image left the keys it follows");
        } else {
            Expect(cache.remade == 199, "the unshared model does not reproduce the map's two remakes a frame");
        }
    }
}

void namedClearRemakes() {
    Keys keys;
    SurfaceCache cache{keys};
    cache.Lookup(TargetKeys);
    keys.ranges[StorageKeys] = DccKeys::Clear0000;
    const auto& image = cache.Lookup(StorageKeys);
    Expect(cache.remade == 1 && image.dcc == StorageKeys && image.uploaded == DccKeys::Clear0000, "a fast clear of the named keys did not reach the surface's image");
    const auto& own = cache.Lookup(StorageKeys);
    Expect(cache.remade == 1 && own.uploaded == DccKeys::Clear0000, "the image of the cleared keys did not serve its own keys");
    cache.Lookup(TargetKeys);
    Expect(cache.remade == 2, "uncompressed keys shared an image holding a clear");
}

void followedClearRemakes() {
    Keys keys;
    SurfaceCache cache{keys};
    cache.Lookup(TargetKeys);
    keys.ranges[TargetKeys] = DccKeys::Clear1111;
    const auto& image = cache.Lookup(StorageKeys);
    Expect(cache.remade == 1 && image.dcc == StorageKeys && image.uploaded == DccKeys::Uncompressed, "a descriptor over uncompressed keys was served an image whose own keys are a clear");
}

void filledKeysRemake() {
    Keys keys;
    SurfaceCache cache{keys};
    cache.Lookup(TargetKeys);
    cache.image->filled = DccKeys::Clear0001;
    cache.Lookup(StorageKeys);
    Expect(cache.remade == 1, "an image holding a key fill served other keys");
}

void serveRules() {
    Keys keys;
    keys.ranges[TargetKeys] = DccKeys::Clear0000;
    Expect(KeysServeSurface(TargetKeys, DccKeys::Clear0000, DccKeys::Uncompressed, 0, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(0); }), "a descriptor without metadata was refused");
    Expect(KeysServeSurface(TargetKeys, DccKeys::Clear0000, DccKeys::Uncompressed, TargetKeys, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(TargetKeys); }), "the image's own keys were refused");
    Expect(keys.reads == 0, "the own-keys and no-metadata answers scanned keys");
    Expect(!KeysServeSurface(TargetKeys, DccKeys::Clear0000, DccKeys::Uncompressed, StorageKeys, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(StorageKeys); }), "an image uploaded under a clear served other keys");
    Expect(keys.reads == 0, "a refusal by the uploaded keys scanned keys");
    keys.ranges[StorageKeys] = DccKeys::Mixed;
    keys.ranges[TargetKeys] = DccKeys::Uncompressed;
    Expect(!KeysServeSurface(TargetKeys, DccKeys::Uncompressed, DccKeys::Uncompressed, StorageKeys, [&] { return keys.Read(TargetKeys); }, [&] { return keys.Read(StorageKeys); }), "keys that do not read uncompressed were served");
    int followedReads = 0;
    Expect(KeysServeSurface(0, DccKeys::Uncompressed, DccKeys::Uncompressed, TargetKeys, [&] { ++followedReads; return DccKeys::Clear0000; }, [&] { return keys.Read(TargetKeys); }), "an image without metadata did not serve uncompressed keys");
    Expect(followedReads == 0, "an image without metadata scanned keys of its own");
}

}

int main() {
    worldMapFrames();
    namedClearRemakes();
    followedClearRemakes();
    filledKeysRemake();
    serveRules();
    if (failures != 0) {
        std::fprintf(stderr, "%d DCC key sharing checks failed\n", failures);
        return 1;
    }
    std::printf("DCC key sharing tests passed\n");
    return 0;
}
