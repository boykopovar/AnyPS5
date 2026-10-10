#include "prx/libSceAgcDriver/Graphics/include/PassHazards.hpp"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <utility>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

using Ranges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
using Images = std::vector<std::pair<VkImage, bool>>;

VkImage image(std::uint64_t id) {
    static_assert(sizeof(VkImage) == sizeof(std::uint64_t));
    VkImage handle = VK_NULL_HANDLE;
    std::memcpy(&handle, &id, sizeof(handle));
    return handle;
}

struct Use {
    Ranges reads;
    Ranges writes;
    Images images;
    Images attachments;
    bool anyReads = false;
    bool anyWrites = false;

    PassAccess View() const { return {reads, writes, images, attachments, anyReads, anyWrites}; }
};

void testRangeSet() {
    GuestRangeSet set;
    Expect(set.Empty() && !set.Overlaps(0, 100), "an empty set overlaps nothing");
    set.Insert(0x1000, 0x2000);
    set.Insert(0x3000, 0x4000);
    Expect(set.Size() == 2, "disjoint ranges stay apart");
    Expect(set.Overlaps(0x1fff, 0x2001) && set.Overlaps(0x0, 0x1001) && set.Overlaps(0x3800, 0x3900), "overlaps inside and at the edges");
    Expect(!set.Overlaps(0x2000, 0x3000) && !set.Overlaps(0x0, 0x1000) && !set.Overlaps(0x4000, 0x5000), "touching ranges do not overlap");
    set.Insert(0x2000, 0x3000);
    Expect(set.Size() == 1 && set.Overlaps(0x2800, 0x2801), "an adjacent range merges both neighbours");
    set.Insert(0x500, 0x5000);
    Expect(set.Size() == 1 && set.Overlaps(0x600, 0x601) && set.Overlaps(0x4fff, 0x5000), "a covering range swallows the set");
    set.Insert(0x9000, 0x9000);
    Expect(set.Size() == 1 && !set.Overlaps(0x9000, 0x9001), "an empty range is ignored");
    set.Clear();
    Expect(set.Empty(), "cleared");
}

void testRanges() {
    PassHazards hazards;
    hazards.BeginPass();
    Use reader;
    reader.reads = {{0x1000, 0x2000}};
    Expect(hazards.Check(reader.View(), true) == PassHazard::None, "nothing recorded: no hazard");
    hazards.Add(reader.View());
    Expect(hazards.Check(reader.View(), true) == PassHazard::None, "read after read");
    Use writer;
    writer.writes = {{0x1800, 0x1900}};
    Expect(hazards.Check(writer.View(), true) == PassHazard::WriteAfterRead, "write after read of the same bytes");
    Use disjoint;
    disjoint.writes = {{0x2000, 0x2100}};
    Expect(hazards.Check(disjoint.View(), true) == PassHazard::None, "write next to a read");
    hazards.Add(disjoint.View());
    Use rereader;
    rereader.reads = {{0x20f0, 0x2200}};
    Expect(hazards.Check(rereader.View(), true) == PassHazard::ReadAfterWrite, "read after write");
    Use rewriter;
    rewriter.writes = {{0x20fc, 0x2100}};
    Expect(hazards.Check(rewriter.View(), true) == PassHazard::WriteAfterWrite, "write after write");
    Use empty;
    empty.reads = {{0x2000, 0x2000}};
    empty.writes = {{0x1000, 0x1000}};
    Expect(hazards.Check(empty.View(), true) == PassHazard::None, "empty ranges touch nothing");
    hazards.Clear();
    Expect(hazards.Empty() && hazards.Check(writer.View(), true) == PassHazard::None, "a barrier clears the accesses");
}

void testUnknown() {
    PassHazards hazards;
    hazards.BeginPass();
    Use addressBased;
    addressBased.anyReads = true;
    Use imageWriter;
    imageWriter.images = {{image(7), true}};
    hazards.Add(imageWriter.View());
    Expect(hazards.Check(addressBased.View(), true) == PassHazard::None, "unknown reads after an image write only");
    hazards.Add(addressBased.View());
    Expect(hazards.Check(addressBased.View(), true) == PassHazard::None, "unknown reads after unknown reads");
    Use writer;
    writer.writes = {{0x100, 0x104}};
    Expect(hazards.Check(writer.View(), true) == PassHazard::UnknownRead, "a write after unknown reads (WAR guard)");
    Use reader;
    reader.reads = {{0x100, 0x104}};
    Expect(hazards.Check(reader.View(), true) == PassHazard::None, "a ranged read after unknown reads");
    hazards.Clear();
    hazards.Add(writer.View());
    Expect(hazards.Check(addressBased.View(), true) == PassHazard::UnknownRead, "unknown reads after a write");
    hazards.Clear();
    Use unknownWriter;
    unknownWriter.anyWrites = true;
    Expect(hazards.Check(unknownWriter.View(), true) == PassHazard::None, "unknown writes first");
    hazards.Add(imageWriter.View());
    Expect(hazards.Check(unknownWriter.View(), true) == PassHazard::None, "unknown writes after an image access only");
    hazards.Add(reader.View());
    Expect(hazards.Check(unknownWriter.View(), true) == PassHazard::UnknownWrite, "unknown writes after a read");
    hazards.Clear();
    hazards.Add(unknownWriter.View());
    Expect(hazards.Check(reader.View(), true) == PassHazard::UnknownWrite, "a read after unknown writes");
    Expect(hazards.Check(addressBased.View(), true) == PassHazard::UnknownWrite, "unknown reads after unknown writes");
    Expect(hazards.Check(imageWriter.View(), true) == PassHazard::None, "an image-only access after unknown writes");
}

void testImages() {
    PassHazards hazards;
    hazards.BeginPass();
    Use sampler;
    sampler.images = {{image(1), false}};
    hazards.Add(sampler.View());
    Expect(hazards.Check(sampler.View(), true) == PassHazard::None, "two reads of an image");
    Use storer;
    storer.images = {{image(1), true}};
    Expect(hazards.Check(storer.View(), true) == PassHazard::Image, "a store after a read of the image");
    Use other;
    other.images = {{image(2), true}};
    Expect(hazards.Check(other.View(), true) == PassHazard::None, "a store to another image");
    hazards.Add(other.View());
    Use otherReader;
    otherReader.images = {{image(2), false}};
    Expect(hazards.Check(otherReader.View(), true) == PassHazard::Image, "a read after a store of the image");
    Use null;
    null.images = {{VK_NULL_HANDLE, true}};
    null.attachments = {{VK_NULL_HANDLE, true}};
    Expect(hazards.Check(null.View(), true) == PassHazard::None, "null images are ignored");
}

void testAttachments() {
    PassHazards hazards;
    hazards.BeginPass();
    Use draw;
    draw.attachments = {{image(10), true}, {image(11), true}};
    hazards.Add(draw.View());
    Expect(hazards.Check(draw.View(), true) == PassHazard::None, "attachment writes of the same pass are in rasterization order");
    Expect(hazards.Check(draw.View(), false) == PassHazard::Image, "the same attachments in a new pass");
    Use readOnlyDepth;
    readOnlyDepth.attachments = {{image(12), false}};
    hazards.Add(readOnlyDepth.View());
    hazards.BeginPass();
    Use otherPass;
    otherPass.attachments = {{image(20), true}, {image(12), false}};
    Expect(hazards.Check(otherPass.View(), false) == PassHazard::None, "a new pass of other targets reading a depth image the last pass only read");
    hazards.Add(otherPass.View());
    Expect(hazards.Check(otherPass.View(), true) == PassHazard::None, "continuing the new pass");
    Use sampleEarlier;
    sampleEarlier.images = {{image(10), false}};
    Expect(hazards.Check(sampleEarlier.View(), true) == PassHazard::Image, "sampling a target of an earlier pass");
    Use backToFirst;
    backToFirst.attachments = {{image(10), true}};
    Expect(hazards.Check(backToFirst.View(), false) == PassHazard::Image, "rendering into a target of an earlier pass");
    hazards.Clear();
    hazards.BeginPass();
    Use sampled;
    sampled.images = {{image(30), false}};
    hazards.Add(sampled.View());
    Use renderInto;
    renderInto.attachments = {{image(30), true}};
    Expect(hazards.Check(renderInto.View(), true) == PassHazard::Image, "rendering into an image a draw of the pass sampled");
    Use storageWrite;
    storageWrite.images = {{image(31), true}};
    hazards.Add(storageWrite.View());
    Use renderIntoStored;
    renderIntoStored.attachments = {{image(31), true}};
    Expect(hazards.Check(renderIntoStored.View(), true) == PassHazard::Image, "rendering into an image a draw stored to");
}

}

int main() {
    testRangeSet();
    testRanges();
    testUnknown();
    testImages();
    testAttachments();
    if (failures != 0) {
        std::fprintf(stderr, "%d pass hazard checks failed\n", failures);
        return 1;
    }
    std::printf("pass hazard tests passed\n");
    return 0;
}
