#include "Optimization/ImageOverwriteProof.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

struct Kernel {
    IrProgram program;
    IrBlock* block = nullptr;

    Kernel() {
        block = &program.CreateBlock();
        program.BlockOrder().push_back(block);
        BlockInfo info;
        info.id = block->Id();
        info.terminator.kind = TerminatorKind::Return;
        program.Metadata().blockInfo.push_back(info);
    }

    IrValue& Immediate(std::uint32_t value) {
        auto& constant = program.CreateValue(IrOpcode::Void, IrType::U32);
        constant.SetImmediateU32(value);
        return constant;
    }

    IrValue& Bool(bool value) {
        auto& constant = program.CreateValue(IrOpcode::Void, IrType::Bool);
        constant.SetImmediateBool(value);
        return constant;
    }

    IrValue& Emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
        auto& value = program.CreateValue(opcode, type);
        for (auto* argument : arguments) value.AddArgument(argument);
        block->AppendInstruction(&value);
        return value;
    }

    IrValue& Builtin(StageInputKind kind, std::uint32_t component) {
        return Emit(IrOpcode::GetBuiltin, IrType::U32, {&Immediate(static_cast<std::uint32_t>(kind)), &Immediate(component)});
    }

    IrValue& UserData(std::uint32_t index) {
        auto& reg = program.CreateValue(IrOpcode::Void, IrType::ScalarReg);
        reg.SetRegister(GuestRegister{RegisterBank::Scalar, index});
        return Emit(IrOpcode::GetUserData, IrType::U32, {&reg});
    }

    IrValue& Tiled(std::uint32_t axis, std::uint32_t shift) {
        auto& shifted = Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&Builtin(StageInputKind::WorkgroupId, axis), &Immediate(shift)});
        return Emit(IrOpcode::IAdd32, IrType::U32, {&shifted, &Builtin(StageInputKind::LocalInvocationId, axis)});
    }

    std::uint32_t Image(ImageResourceClass resourceClass, bool written, bool read, std::uint32_t source = 0) {
        ImageResource image;
        image.source = source;
        image.resourceClass = resourceClass;
        image.dimension = RdnaImageDimension::Dim2D;
        image.written = written;
        image.read = read;
        image.dimensionQueryOnly = !read && !written;
        program.Resources().info.images.push_back(image);
        return static_cast<std::uint32_t>(program.Resources().info.images.size() - 1);
    }

    void Store(std::uint32_t image, IrValue& x, IrValue& y, IrValue& exec) {
        MemoryInfo memory;
        memory.kind = ResourceKind::Image;
        memory.resource = image;
        memory.imageDimension = RdnaImageDimension::Dim2D;
        memory.imageAddressComponents = 2;
        program.Resources().memoryInfo.push_back(memory);
        auto& address = Emit(IrOpcode::MakeImageAddress, IrType::Opaque, {&x, &y});
        auto& data = Emit(IrOpcode::CompositeConstructU32x4, IrType::Vec4U32, {&Immediate(0), &Immediate(0), &Immediate(0), &Immediate(0)});
        auto& resource = Emit(IrOpcode::GetImageResource, IrType::Opaque, {&UserData(0)});
        auto& store = Emit(IrOpcode::ImageWrite, IrType::Void, {&resource, &address, &data, &exec});
        store.SetFlags(MemoryFlags{static_cast<std::uint32_t>(program.Resources().memoryInfo.size() - 1), 0});
    }

    const ImageOverwrite& Prove(std::uint32_t image, std::array<std::uint32_t, 3> threads = {8, 8, 1}) {
        ProveImageOverwrites(program, threads);
        return program.Resources().info.images.at(image).overwrite;
    }
};

ImageOverwriteDispatch Grid(std::uint32_t x, std::uint32_t y) {
    ImageOverwriteDispatch dispatch;
    dispatch.groups = {x, y, 1};
    dispatch.userCount = 16;
    return dispatch;
}

void GlobalIdStore() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    kernel.Store(image, kernel.Tiled(0, 3), kernel.Tiled(1, 3), kernel.Bool(true));
    const auto& proof = kernel.Prove(image);
    Require(proof.proved, "a store at the global thread id of every texel is not proved");
    Require(proof.axes[0].groupAxis == 0u && proof.axes[0].tile == 8u && proof.axes[1].groupAxis == 1u && proof.axes[1].tile == 8u, "the proof's tiles are not 8x8 on group axes x and y");
    Require(ImageOverwriteCovers(proof, Grid(200, 113), 1600, 900), "a 200x113 grid of 8x8 groups does not cover a 1600x900 image");
    Require(!ImageOverwriteCovers(proof, Grid(200, 112), 1600, 900), "a 200x112 grid of 8x8 groups covers 900 rows");
    auto partial = Grid(200, 113);
    partial.partialThreads = {0, 0, 0};
    Require(ImageOverwriteCovers(proof, partial, 1600, 900), "a dispatch without partial groups is refused");
}

void ReadImage() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    kernel.Image(ImageResourceClass::Sampled, false, true);
    kernel.Store(image, kernel.Tiled(0, 3), kernel.Tiled(1, 3), kernel.Bool(true));
    Require(!kernel.Prove(image).proved, "a store to an image the shader also reads is proved");
}

void SparseTile() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    kernel.Store(image, kernel.Tiled(0, 4), kernel.Tiled(1, 3), kernel.Bool(true));
    Require(!kernel.Prove(image).proved, "8 threads per 16-texel tile are proved to cover it");
}

void TwoStoresPerThread() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& x = kernel.Tiled(0, 4);
    auto& y = kernel.Tiled(1, 3);
    kernel.Store(image, x, y, kernel.Bool(true));
    kernel.Store(image, kernel.Emit(IrOpcode::IAdd32, IrType::U32, {&x, &kernel.Immediate(8)}), y, kernel.Bool(true));
    const auto& proof = kernel.Prove(image);
    Require(proof.proved && proof.axes[0].tile == 16u, "two stores per thread that fill a 16x8 tile are not proved");
}

void LaneGuard() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& guard = kernel.Emit(IrOpcode::ULessThan32, IrType::Bool, {&kernel.Builtin(StageInputKind::LocalInvocationId, 0), &kernel.Immediate(4)});
    kernel.Store(image, kernel.Tiled(0, 3), kernel.Tiled(1, 3), guard);
    Require(!kernel.Prove(image).proved, "a store that half of each group's lanes skip is proved");
}

void UserDataRect() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& x = kernel.Tiled(0, 3);
    auto& y = kernel.Tiled(1, 3);
    auto& inX = kernel.Emit(IrOpcode::UGreaterThan32, IrType::Bool, {&kernel.UserData(6), &x});
    auto& inY = kernel.Emit(IrOpcode::UGreaterThan32, IrType::Bool, {&kernel.UserData(7), &y});
    auto& exec = kernel.Emit(IrOpcode::LogicalAnd, IrType::Bool, {&inX, &inY});
    auto& offsetX = kernel.Emit(IrOpcode::IAdd32, IrType::U32, {&kernel.UserData(4), &x});
    auto& offsetY = kernel.Emit(IrOpcode::IAdd32, IrType::U32, {&kernel.UserData(5), &y});
    kernel.Store(image, offsetX, offsetY, exec);
    const auto& proof = kernel.Prove(image);
    Require(proof.proved, "a rectangle store with user-data offset and size is not proved");
    Require(proof.axes[0].offsetRegister == 4u && proof.axes[0].boundRegister == 6u && proof.axes[1].offsetRegister == 5u && proof.axes[1].boundRegister == 7u, "the rectangle's offset and size registers are wrong");
    auto dispatch = Grid(200, 113);
    dispatch.userData[6] = 1600;
    dispatch.userData[7] = 900;
    Require(ImageOverwriteCovers(proof, dispatch, 1600, 900), "a 1600x900 rectangle at the origin does not cover the image");
    dispatch.userData[4] = 8;
    Require(!ImageOverwriteCovers(proof, dispatch, 1600, 900), "a rectangle 8 texels right of the origin covers the image");
    dispatch.userData[4] = 0;
    dispatch.userData[7] = 899;
    Require(!ImageOverwriteCovers(proof, dispatch, 1600, 900), "a rectangle one row short covers the image");
    dispatch.userData[7] = 900;
    dispatch.userCount = 7;
    Require(!ImageOverwriteCovers(proof, dispatch, 1600, 900), "a size register past the dispatch's user data is trusted");
}

void Diagonal() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& x = kernel.Tiled(0, 3);
    auto& y = kernel.Emit(IrOpcode::IAdd32, IrType::U32, {&kernel.Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&kernel.Builtin(StageInputKind::WorkgroupId, 0), &kernel.Immediate(3)}), &kernel.Builtin(StageInputKind::LocalInvocationId, 1)});
    kernel.Store(image, x, y, kernel.Bool(true));
    Require(!kernel.Prove(image).proved, "a store whose rows and columns both follow group x is proved");
}

void LooseBound() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& x = kernel.Tiled(0, 3);
    auto& ahead = kernel.Emit(IrOpcode::IAdd32, IrType::U32, {&x, &kernel.Immediate(1)});
    auto& inX = kernel.Emit(IrOpcode::UGreaterThan32, IrType::Bool, {&kernel.UserData(6), &ahead});
    kernel.Store(image, x, kernel.Tiled(1, 3), inX);
    Require(!kernel.Prove(image).proved, "a bound one texel ahead of the coordinate is proved to cover every texel below it");
}

void WrappingBound() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& x = kernel.Tiled(0, 3);
    auto& behind = kernel.Emit(IrOpcode::ISub32, IrType::U32, {&x, &kernel.Immediate(5)});
    auto& inX = kernel.Emit(IrOpcode::ULessThan32, IrType::Bool, {&behind, &kernel.Immediate(100)});
    kernel.Store(image, x, kernel.Tiled(1, 3), inX);
    Require(!kernel.Prove(image).proved, "a bound whose term wraps below zero for the first lanes is proved to cover them");
}

void EmptyGrid() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    kernel.Store(image, kernel.Tiled(0, 3), kernel.Tiled(1, 3), kernel.Bool(true));
    const auto& proof = kernel.Prove(image);
    Require(proof.proved, "a store at the global thread id of every texel is not proved");
    auto empty = Grid(200, 113);
    empty.groups[2] = 0;
    Require(!ImageOverwriteCovers(proof, empty, 1600, 900), "a dispatch with no groups on the third axis covers the image");
}

void FullGroupsPartialDispatch() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& inX = kernel.Emit(IrOpcode::ULessThan32, IrType::Bool, {&kernel.Builtin(StageInputKind::GlobalInvocationId, 0), &kernel.Builtin(StageInputKind::DispatchThreadLimit, 0)});
    kernel.Store(image, kernel.Tiled(0, 3), kernel.Tiled(1, 3), inX);
    const auto& proof = kernel.Prove(image);
    Require(proof.proved && proof.fullGroups == 1u, "a store guarded by the dispatch's thread limit on x is not proved for full groups");
    Require(ImageOverwriteCovers(proof, Grid(200, 113), 1600, 900), "a dispatch of full groups is refused");
    auto partial = Grid(200, 113);
    partial.partialThreads = {4, 0, 0};
    Require(!ImageOverwriteCovers(proof, partial, 1600, 900), "a dispatch whose last group on x is partial covers the image");
}

void WrappedTruncation() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& far = kernel.Emit(IrOpcode::IAdd32, IrType::U32, {&kernel.Tiled(0, 3), &kernel.Immediate(0x10000u)});
    auto& wrapped = kernel.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&far, &kernel.Immediate(0xffffu)});
    kernel.Store(image, wrapped, kernel.Tiled(1, 3), kernel.Bool(true));
    Require(!kernel.Prove(image).proved, "a 16-bit truncation of a coordinate past 65535 is proved");
}

void Branched(BranchCondition condition, bool skipsOnStoreMask, bool proved, const char* what) {
    Kernel kernel;
    auto& entry = *kernel.block;
    auto& body = kernel.program.CreateBlock();
    auto& exit = kernel.program.CreateBlock();
    for (auto* block : {&body, &exit}) kernel.program.BlockOrder().push_back(block);
    entry.AddSuccessor(&exit);
    entry.AddSuccessor(&body);
    body.AddPredecessor(&entry);
    body.AddSuccessor(&exit);
    exit.AddPredecessor(&entry);
    exit.AddPredecessor(&body);
    auto& infos = kernel.program.Metadata().blockInfo;
    infos[0].terminator.kind = TerminatorKind::ConditionalBranch;
    infos[0].terminator.condition = condition;
    infos[0].terminator.trueBlock = exit.Id();
    infos[0].terminator.falseBlock = body.Id();
    BlockInfo bodyInfo;
    bodyInfo.id = body.Id();
    bodyInfo.terminator.kind = TerminatorKind::Branch;
    bodyInfo.terminator.trueBlock = exit.Id();
    infos.push_back(bodyInfo);
    BlockInfo exitInfo;
    exitInfo.id = exit.Id();
    infos.push_back(exitInfo);
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& x = kernel.Tiled(0, 3);
    auto& y = kernel.Tiled(1, 3);
    auto& inX = kernel.Emit(IrOpcode::UGreaterThan32, IrType::Bool, {&kernel.UserData(6), &x});
    auto& other = kernel.Emit(IrOpcode::UGreaterThan32, IrType::Bool, {&kernel.UserData(7), &y});
    auto& skipped = kernel.Emit(IrOpcode::LogicalNot, IrType::Bool, {skipsOnStoreMask ? &inX : &other});
    kernel.Emit(IrOpcode::Reference, IrType::Void, {&skipped});
    kernel.block = &body;
    kernel.Store(image, x, y, inX);
    Require(kernel.Prove(image).proved == proved, what);
}

void PackedCoordinates() {
    Kernel kernel;
    const auto image = kernel.Image(ImageResourceClass::Storage, true, false);
    auto& low = kernel.Emit(IrOpcode::BitwiseAnd32, IrType::U32, {&kernel.Tiled(0, 3), &kernel.Immediate(0xffffu)});
    auto& high = kernel.Emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&kernel.Tiled(1, 3), &kernel.Immediate(16)});
    auto& packed = kernel.Emit(IrOpcode::BitwiseOr32, IrType::U32, {&low, &high});
    MemoryInfo memory;
    memory.kind = ResourceKind::Image;
    memory.resource = image;
    memory.imageDimension = RdnaImageDimension::Dim2D;
    memory.imageAddressComponents = 2;
    memory.imageSampleFlags = RdnaImageSampleFlagA16;
    kernel.program.Resources().memoryInfo.push_back(memory);
    auto& address = kernel.Emit(IrOpcode::MakeImageAddress, IrType::Opaque, {&packed});
    auto& data = kernel.Emit(IrOpcode::CompositeConstructU32x4, IrType::Vec4U32, {&kernel.Immediate(0), &kernel.Immediate(0), &kernel.Immediate(0), &kernel.Immediate(0)});
    auto& resource = kernel.Emit(IrOpcode::GetImageResource, IrType::Opaque, {&kernel.UserData(0)});
    auto& store = kernel.Emit(IrOpcode::ImageWrite, IrType::Void, {&resource, &address, &data, &kernel.Bool(true)});
    store.SetFlags(MemoryFlags{static_cast<std::uint32_t>(kernel.program.Resources().memoryInfo.size() - 1), 0});
    const auto& proof = kernel.Prove(image);
    Require(proof.proved, "a store at 16-bit packed global ids is not proved");
    Require(ImageOverwriteCovers(proof, Grid(200, 113), 1600, 900), "packed coordinates of a 1600x900 grid are refused");
    Require(!ImageOverwriteCovers(proof, Grid(8193, 113), 1600, 900), "packed coordinates past 65535 are accepted");
}

}

int main() {
    try {
        GlobalIdStore();
        ReadImage();
        SparseTile();
        TwoStoresPerThread();
        LaneGuard();
        UserDataRect();
        Diagonal();
        LooseBound();
        Branched(BranchCondition::ExecZero, true, true, "a store skipped only when no lane of the wave would store is not proved");
        Branched(BranchCondition::SccZero, true, false, "a store behind a scalar branch is proved");
        Branched(BranchCondition::ExecZero, false, false, "a store skipped on another mask is proved");
        WrappedTruncation();
        PackedCoordinates();
        WrappingBound();
        EmptyGrid();
        FullGroupsPartialDispatch();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "image overwrite proof: %s\n", error.what());
        return 1;
    }
    std::puts("image overwrite proof: ok");
    return 0;
}
