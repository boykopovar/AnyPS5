#include "../FunctionLifter.hpp"

#include <llvm/IR/IntrinsicsAArch64.h>
#include <llvm/IR/IntrinsicsX86.h>

#include <array>
#include <initializer_list>

namespace Ps5Lift::Lifting {

void FunctionLifter::Crc32() {
    auto* crc = _builder.CreateTrunc(Read(0), _builder.getInt32Ty());
    auto* data = Read(1);
    const auto sourceBits = data->getType()->getIntegerBitWidth();
    llvm::Value* result = nullptr;
    if (_module.Triple().isAArch64()) {
        const auto id = sourceBits == 8 ? llvm::Intrinsic::aarch64_crc32cb : sourceBits == 16 ? llvm::Intrinsic::aarch64_crc32ch : sourceBits == 32 ? llvm::Intrinsic::aarch64_crc32cw : llvm::Intrinsic::aarch64_crc32cx;
        result = _builder.CreateIntrinsic(id, {}, {crc, sourceBits == 64 ? data : _builder.CreateZExt(data, _builder.getInt32Ty())});
    } else {
        const auto id = sourceBits == 8 ? llvm::Intrinsic::x86_sse42_crc32_32_8 : sourceBits == 16 ? llvm::Intrinsic::x86_sse42_crc32_32_16 : sourceBits == 32 ? llvm::Intrinsic::x86_sse42_crc32_32_32 : llvm::Intrinsic::x86_sse42_crc32_64_64;
        result = _builder.CreateTrunc(_builder.CreateIntrinsic(id, {}, {sourceBits == 64 ? _builder.CreateZExt(crc, I64()) : crc, data}), _builder.getInt32Ty());
    }
    Write(0, _builder.CreateZExt(result, Int(Bits(0))));
}

void FunctionLifter::Crypto(const ZydisMnemonic mnemonic) {
    if (Bits(0) != 128) throw UnsupportedInstruction("VAES and VPCLMULQDQ on 256-bit registers");
    const bool arm = _module.Triple().isAArch64();
    auto* zero = llvm::Constant::getNullValue(llvm::FixedVectorType::get(Int(8), 16));
    const auto bytes = [&](llvm::Value* value) { return AsVector(value, 8); };
    const auto quads = [&](llvm::Value* value) { return AsVector(value, 64); };
    const auto intrinsic = [&](const llvm::Intrinsic::ID id, std::initializer_list<llvm::Value*> arguments) { return _builder.CreateIntrinsic(id, {}, arguments); };
    llvm::Value* result = nullptr;
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_AESIMC:
    case ZYDIS_MNEMONIC_VAESIMC:
        result = arm ? intrinsic(llvm::Intrinsic::aarch64_crypto_aesimc, {bytes(ReadAs(1, 128))}) : intrinsic(llvm::Intrinsic::x86_aesni_aesimc, {quads(ReadAs(1, 128))});
        break;
    case ZYDIS_MNEMONIC_AESKEYGENASSIST:
    case ZYDIS_MNEMONIC_VAESKEYGENASSIST: {
        auto* source = ReadAs(1, 128);
        const auto constant = Immediate(2);
        if (!arm) {
            result = intrinsic(llvm::Intrinsic::x86_aesni_aeskeygenassist, {quads(source), _builder.getInt8(static_cast<std::uint8_t>(constant))});
            break;
        }
        static constexpr int InverseShiftRows[16] = {0, 13, 10, 7, 4, 1, 14, 11, 8, 5, 2, 15, 12, 9, 6, 3};
        auto* unshifted = _builder.CreateShuffleVector(bytes(source), bytes(source), InverseShiftRows);
        auto* substituted = AsVector(AsInteger(intrinsic(llvm::Intrinsic::aarch64_crypto_aese, {unshifted, zero})), 32);
        auto* words = _builder.CreateShuffleVector(substituted, substituted, {1, 1, 3, 3});
        auto* rotated = _builder.CreateIntrinsic(llvm::Intrinsic::fshr, {words->getType()}, {words, words, Splat(words, _builder.getInt32(8))});
        auto* round = _builder.CreateXor(rotated, Splat(words, _builder.getInt32(static_cast<std::uint32_t>(constant & 0xff))));
        result = _builder.CreateShuffleVector(words, round, {0, 5, 2, 7});
        break;
    }
    case ZYDIS_MNEMONIC_PCLMULQDQ:
    case ZYDIS_MNEMONIC_VPCLMULQDQ: {
        const int base = Base(3);
        auto* a = quads(ReadAs(base, 128));
        auto* b = quads(ReadAs(base + 1, 128));
        const auto select = Immediate(base + 2);
        if (!arm) {
            result = intrinsic(llvm::Intrinsic::x86_pclmulqdq, {a, b, _builder.getInt8(static_cast<std::uint8_t>(select))});
            break;
        }
        result = intrinsic(llvm::Intrinsic::aarch64_neon_pmull64, {_builder.CreateExtractElement(a, _builder.getInt64(select & 1)), _builder.CreateExtractElement(b, _builder.getInt64((select >> 4) & 1))});
        break;
    }
    case ZYDIS_MNEMONIC_AESENC: case ZYDIS_MNEMONIC_VAESENC: case ZYDIS_MNEMONIC_AESENCLAST: case ZYDIS_MNEMONIC_VAESENCLAST:
    case ZYDIS_MNEMONIC_AESDEC: case ZYDIS_MNEMONIC_VAESDEC: case ZYDIS_MNEMONIC_AESDECLAST: case ZYDIS_MNEMONIC_VAESDECLAST: {
        const int base = Base(2);
        auto* state = ReadAs(base, 128);
        auto* key = ReadAs(base + 1, 128);
        if (!arm) {
            const auto id = mnemonic == ZYDIS_MNEMONIC_AESENC || mnemonic == ZYDIS_MNEMONIC_VAESENC         ? llvm::Intrinsic::x86_aesni_aesenc
                            : mnemonic == ZYDIS_MNEMONIC_AESENCLAST || mnemonic == ZYDIS_MNEMONIC_VAESENCLAST ? llvm::Intrinsic::x86_aesni_aesenclast
                            : mnemonic == ZYDIS_MNEMONIC_AESDEC || mnemonic == ZYDIS_MNEMONIC_VAESDEC         ? llvm::Intrinsic::x86_aesni_aesdec
                                                                                                              : llvm::Intrinsic::x86_aesni_aesdeclast;
            result = intrinsic(id, {quads(state), quads(key)});
            break;
        }
        const bool encrypt = mnemonic == ZYDIS_MNEMONIC_AESENC || mnemonic == ZYDIS_MNEMONIC_VAESENC || mnemonic == ZYDIS_MNEMONIC_AESENCLAST || mnemonic == ZYDIS_MNEMONIC_VAESENCLAST;
        const bool last = mnemonic == ZYDIS_MNEMONIC_AESENCLAST || mnemonic == ZYDIS_MNEMONIC_VAESENCLAST || mnemonic == ZYDIS_MNEMONIC_AESDECLAST || mnemonic == ZYDIS_MNEMONIC_VAESDECLAST;
        auto* round = intrinsic(encrypt ? llvm::Intrinsic::aarch64_crypto_aese : llvm::Intrinsic::aarch64_crypto_aesd, {bytes(state), zero});
        if (!last) round = intrinsic(encrypt ? llvm::Intrinsic::aarch64_crypto_aesmc : llvm::Intrinsic::aarch64_crypto_aesimc, {round});
        result = _builder.CreateXor(round, bytes(key));
        break;
    }
    default:
        UnexpectedMnemonic("Crypto", mnemonic);
    }
    Write(0, AsInteger(result));
}

void FunctionLifter::Sha(const ZydisMnemonic mnemonic) {
    auto* first = AsVector(ReadAs(0, 128), 32);
    auto* second = AsVector(ReadAs(1, 128), 32);
    const auto lane = [&](llvm::Value* vector, const int index) { return _builder.CreateExtractElement(vector, _builder.getInt64(index)); };
    const auto rotate = [&](llvm::Value* value, const unsigned left) { return _builder.CreateIntrinsic(llvm::Intrinsic::fshl, {Int(32)}, {value, value, _builder.getInt32(left % 32)}); };
    const auto rotateRight = [&](llvm::Value* value, const unsigned right) { return rotate(value, 32 - right); };
    const auto exclusive = [&](llvm::Value* a, llvm::Value* b, llvm::Value* c) { return _builder.CreateXor(_builder.CreateXor(a, b), c); };
    const auto majority = [&](llvm::Value* a, llvm::Value* b, llvm::Value* c) {
        return exclusive(_builder.CreateAnd(a, b), _builder.CreateAnd(a, c), _builder.CreateAnd(b, c));
    };
    const auto choose = [&](llvm::Value* e, llvm::Value* f, llvm::Value* g) { return _builder.CreateXor(_builder.CreateAnd(e, f), _builder.CreateAnd(_builder.CreateNot(e), g)); };
    const auto sum = [&](std::initializer_list<llvm::Value*> values) {
        llvm::Value* total = nullptr;
        for (auto* value : values) total = total ? _builder.CreateAdd(total, value) : value;
        return total;
    };
    std::array<llvm::Value*, 4> lanes{};
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_SHA1RNDS4: {
        const auto function = Immediate(2) & 3;
        static constexpr std::uint32_t Constants[4] = {0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6};
        const auto round = [&](llvm::Value* b, llvm::Value* c, llvm::Value* d) {
            if (function == 0) return choose(b, c, d);
            if (function == 2) return majority(b, c, d);
            return exclusive(b, c, d);
        };
        llvm::Value* a = lane(first, 3);
        llvm::Value* b = lane(first, 2);
        llvm::Value* c = lane(first, 1);
        llvm::Value* d = lane(first, 0);
        llvm::Value* e = nullptr;
        for (int i = 0; i < 4; ++i) {
            llvm::Value* next = sum({round(b, c, d), rotate(a, 5), lane(second, 3 - i), _builder.getInt32(Constants[function])});
            if (e != nullptr) next = _builder.CreateAdd(next, e);
            e = d;
            d = c;
            c = rotate(b, 30);
            b = a;
            a = next;
        }
        lanes = {d, c, b, a};
        break;
    }
    case ZYDIS_MNEMONIC_SHA1NEXTE:
        lanes = {lane(second, 0), lane(second, 1), lane(second, 2), _builder.CreateAdd(lane(second, 3), rotate(lane(first, 3), 30))};
        break;
    case ZYDIS_MNEMONIC_SHA1MSG1:
        lanes = {_builder.CreateXor(lane(second, 2), lane(first, 0)), _builder.CreateXor(lane(second, 3), lane(first, 1)), _builder.CreateXor(lane(first, 0), lane(first, 2)),
                 _builder.CreateXor(lane(first, 1), lane(first, 3))};
        break;
    case ZYDIS_MNEMONIC_SHA1MSG2: {
        auto* w16 = rotate(_builder.CreateXor(lane(first, 3), lane(second, 2)), 1);
        auto* w17 = rotate(_builder.CreateXor(lane(first, 2), lane(second, 1)), 1);
        auto* w18 = rotate(_builder.CreateXor(lane(first, 1), lane(second, 0)), 1);
        auto* w19 = rotate(_builder.CreateXor(lane(first, 0), w16), 1);
        lanes = {w19, w18, w17, w16};
        break;
    }
    case ZYDIS_MNEMONIC_SHA256RNDS2: {
        auto* keyed = AsVector(ReadRegister(ZYDIS_REGISTER_XMM0), 32);
        llvm::Value* a = lane(second, 3);
        llvm::Value* b = lane(second, 2);
        llvm::Value* c = lane(first, 3);
        llvm::Value* d = lane(first, 2);
        llvm::Value* e = lane(second, 1);
        llvm::Value* f = lane(second, 0);
        llvm::Value* g = lane(first, 1);
        llvm::Value* h = lane(first, 0);
        for (int i = 0; i < 2; ++i) {
            auto* common = sum({choose(e, f, g), exclusive(rotateRight(e, 6), rotateRight(e, 11), rotateRight(e, 25)), lane(keyed, i), h});
            auto* nextA = sum({common, majority(a, b, c), exclusive(rotateRight(a, 2), rotateRight(a, 13), rotateRight(a, 22))});
            auto* nextE = _builder.CreateAdd(common, d);
            h = g;
            g = f;
            f = e;
            e = nextE;
            d = c;
            c = b;
            b = a;
            a = nextA;
        }
        lanes = {f, e, b, a};
        break;
    }
    case ZYDIS_MNEMONIC_SHA256MSG1: {
        const auto sigma = [&](llvm::Value* w) { return exclusive(rotateRight(w, 7), rotateRight(w, 18), _builder.CreateLShr(w, 3)); };
        lanes = {_builder.CreateAdd(lane(first, 0), sigma(lane(first, 1))), _builder.CreateAdd(lane(first, 1), sigma(lane(first, 2))),
                 _builder.CreateAdd(lane(first, 2), sigma(lane(first, 3))), _builder.CreateAdd(lane(first, 3), sigma(lane(second, 0)))};
        break;
    }
    case ZYDIS_MNEMONIC_SHA256MSG2: {
        const auto sigma = [&](llvm::Value* w) { return exclusive(rotateRight(w, 17), rotateRight(w, 19), _builder.CreateLShr(w, 10)); };
        auto* w16 = _builder.CreateAdd(lane(first, 0), sigma(lane(second, 2)));
        auto* w17 = _builder.CreateAdd(lane(first, 1), sigma(lane(second, 3)));
        auto* w18 = _builder.CreateAdd(lane(first, 2), sigma(w16));
        auto* w19 = _builder.CreateAdd(lane(first, 3), sigma(w17));
        lanes = {w16, w17, w18, w19};
        break;
    }
    default:
        UnexpectedMnemonic("Sha", mnemonic);
    }
    llvm::Value* result = llvm::PoisonValue::get(first->getType());
    for (int i = 0; i < 4; ++i) result = _builder.CreateInsertElement(result, lanes[i], _builder.getInt64(i));
    Write(0, AsInteger(result));
}

}
