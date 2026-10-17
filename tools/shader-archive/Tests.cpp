#include "Archive.hpp"
#include "Native.hpp"
#include <iostream>

using namespace ShaderArchive;
template <class F> void Reject(F &&f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    Check(failed, "expected rejection");
}
int main() {
    try {
        const std::string abc = "abc";
        Check(Sha256({reinterpret_cast<const std::uint8_t *>(abc.data()), abc.size()}) ==
                  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              "SHA256 known answer");
        Bytes b{1, 2, 3, 4};
        Check(U32(b, 0) == 0x04030201 && ReadInt(b, 0, 4, true) == 0x01020304, "integer endian");
        Reject([&] { Slice(b, 4, 1); });
        Reject([&] { U64(b, 0); });
        Bytes compressed{0xcc, 10};
        compressed.insert(compressed.end(), 0x40000, 0x31);
        compressed.push_back(0xcc);
        compressed.push_back(10);
        compressed.insert(compressed.end(), 37, 0x72);
        auto decoded = Decompress(compressed, 0x40000 + 37);
        Check(decoded.size() == 0x40000 + 37 && decoded[0] == 0x31 && decoded[0x40000] == 0x72, "Oodle block restart");
        Reject([&] { Decompress(Bytes{0xcc, 10, 1, 2}, 16); });
        Reject([&] { Decompress(Bytes{0xcc, 10, 1, 2, 3, 4}, 3); });
        Reject([&] { Decompress(Bytes{0x8c}, 10); });
        for (unsigned round = 0; round < 16; ++round)
            Parallel(64, 16, [&](std::size_t, unsigned) {
                Check(Decompress(Bytes{0xcc, 10, 1, 2, 3, 4}, 4) == Bytes({1, 2, 3, 4}),
                      "parallel decoder lifetime");
            });
        const auto be24 = [](Bytes &out, std::size_t v) {
            out.push_back(static_cast<std::uint8_t>(v >> 16));
            out.push_back(static_cast<std::uint8_t>(v >> 8));
            out.push_back(static_cast<std::uint8_t>(v));
        };
        const auto rawArray = [&](Bytes &out, const Bytes &data) {
            be24(out, data.size());
            out.insert(out.end(), data.begin(), data.end());
        };
        const auto text = [](const Bytes &b) { return std::string(b.begin(), b.end()); };
        const auto wrap = [&](const Bytes &payload) {
            Bytes s{0x0c, 10};
            be24(s, payload.size() - 1);
            s.insert(s.end(), payload.begin(), payload.end());
            return s;
        };
        {
            Bytes payload;
            const Bytes body{0x30, 0x00, 0xfc, 0x00, 0x01, 'x'};
            payload = body;
            Check(text(Decompress(wrap(payload), 64)) == std::string(64, 'x'), "Oodle entropy RLE memset");
        }
        {
            Bytes body;
            const std::string prefix = "ABCDEFGH";
            body.insert(body.end(), prefix.begin(), prefix.end());
            rawArray(body, Bytes{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':'});
            rawArray(body, Bytes{0xf8, 0xf8, 0xf8});
            body.insert(body.end(), {0, 0, 0, 0, 0});
            Bytes payload;
            be24(payload, 0x800000 | (1 << 19) | body.size());
            payload.insert(payload.end(), body.begin(), body.end());
            const auto out = Decompress(wrap(payload), 64);
            std::string expected(53, ' ');
            for (std::size_t i = 0; i < expected.size(); ++i)
                expected[i] = prefix[i % 8];
            expected += "0123456789:";
            Check(text(out) == expected, "Oodle Mermaid match copy");
        }
        {
            Bytes body;
            const std::string prefix = "ABCDEFGH";
            body.insert(body.end(), prefix.begin(), prefix.end());
            rawArray(body, Bytes(26, 1));
            rawArray(body, Bytes{0xff, 0xff});
            body.insert(body.end(), {0, 0, 0, 0, 0});
            Bytes payload;
            be24(payload, 0x800000 | body.size());
            payload.insert(payload.end(), body.begin(), body.end());
            const auto out = Decompress(wrap(payload), 64);
            Bytes expected(80, 0);
            std::memcpy(expected.data(), prefix.data(), 8);
            std::size_t dst = 8;
            const auto add = [&](std::size_t n) {
                for (std::size_t base = 0; base < n; base += 8) {
                    std::array<std::uint8_t, 8> sum{};
                    for (std::size_t i = 0; i < 8; ++i)
                        sum[i] = static_cast<std::uint8_t>(1 + expected[dst + base + i - 8]);
                    std::memcpy(expected.data() + dst + base, sum.data(), 8);
                }
                dst += n;
            };
            const auto match = [&](std::size_t n) {
                for (std::size_t base = 0; base < n; base += 8) {
                    std::array<std::uint8_t, 8> tmp{};
                    std::memcpy(tmp.data(), expected.data() + dst + base - 8, 8);
                    std::memcpy(expected.data() + dst + base, tmp.data(), 8);
                }
                dst += n;
            };
            add(7);
            match(15);
            add(7);
            match(15);
            add(12);
            expected.resize(64);
            Check(out == expected, "Oodle Mermaid add literals");
        }
        Reject([&] { Decompress(Bytes{0x0c, 6, 0, 0, 0}, 16); });
        {
            Bytes payload;
            be24(payload, 0x800000 | (1 << 19) | 4);
            payload.insert(payload.end(), {1, 2, 3, 4});
            Reject([&] { Decompress(wrap(payload), 64); });
        }
        Reject([&] { ParseNative(Bytes(128)); });
        Bytes blob(20 + 96 + 32);
        auto put = [&](std::size_t at, std::uint64_t v, unsigned n) {
            for (unsigned i = 0; i < n; ++i)
                blob.at(at + i) = v >> (8 * i);
        };
        put(0, 96, 8);
        put(8, 32, 8);
        std::memcpy(blob.data() + 20, "1234", 4);
        put(24, 24, 4);
        put(84, 96, 4);
        put(88, 32, 4);
        put(116, 0xbf810000, 4);
        std::memcpy(blob.data() + 124, "barefoot", 8);
        put(144, 4, 4);
        auto native = ParseNative(blob);
        Check(native.size() == 1 && native[0].codeSize == 32 && native[0].executableSize == 4, "native bounds");
        put(8, 36, 8);
        Reject([&] { ParseNative(blob); });
        put(8, 32, 8);
        const auto temp = fs::temp_directory_path() /
                          ("shader-archive-test-" + std::to_string(Clock::now().time_since_epoch().count()));
        fs::create_directories(temp / "groups");
        const std::string groupId = "00112233445566778899aa09";
        const auto groupFile = temp / "groups" / (groupId + ".bin");
        const auto indexFile = temp / "programs.idx";
        Archive archive;
        archive.groups.push_back({groupId, std::make_shared<Bytes>(blob)});
        archive.entries.push_back({"fixture", "fixture.utoc", "hash", 0, 0, 0, blob.size(), 0});
        const auto programs = Programs(archive, 1);
        WriteAtomic(groupFile, blob);
        WriteProgramIndex(indexFile, programs);
        Json::Value indexed;
        const auto selected = LoadProgramIndex(indexFile, temp, {programs[0].id}, 0, 0, 0, 2, indexed);
        Check(selected.size() == 1 && selected[0].native.codeHash == native[0].codeHash &&
                  indexed["groups_read"].asUInt() == 1,
              "indexed byte verification");
        Check(LoadProgramIndex(indexFile, temp, {programs[0].native.codeHash}, 0, 0, 0, 1, indexed).size() == 1,
              "code-hash index selection");
        Reject([&] { LoadProgramIndex(indexFile, temp, {std::string(64, 'f')}, 0, 0, 0, 1, indexed); });
        blob[116] ^= 1;
        WriteAtomic(groupFile, blob);
        Reject([&] { LoadProgramIndex(indexFile, temp, {programs[0].id}, 0, 0, 0, 1, indexed); });
        WriteAtomic(indexFile, Bytes(40));
        Reject([&] { LoadProgramIndex(indexFile, temp, {}, 0, 0, 0, 1, indexed); });
        fs::remove(indexFile);
        fs::remove(groupFile);
        fs::remove(temp / "groups");
        fs::remove(temp);
        std::atomic_uint total{0};
        Parallel(1000, 8, [&](auto, unsigned) { ++total; });
        Check(total == 1000, "parallel accounting");
        Reject([&] {
            Parallel(100, 4, [](auto i, unsigned) {
                if (i == 3)
                    throw std::runtime_error("worker");
            });
        });
        std::cout << "shader archive checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
