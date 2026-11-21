#ifndef CORE_LIBS_PRX_LIBSCEAGC_DRAWQUEUERESETORACLE_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DRAWQUEUERESETORACLE_HPP
#include <array>
#include <cstdint>
#include <span>
namespace DrawResetOracle {
struct Case { std::uint32_t flags; std::span<const std::uint32_t> words; };
inline constexpr std::array<std::uint32_t, 8> Words0{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words1{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words2{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words3{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words4{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words5{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce201000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 10> Words6{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200100u, 0x00000000u, 0xc0004600u, 0x00000410u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 16> Words7{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200010u, 0x00000000u, 0xc0017a00u, 0x20000243u, 0x00000480u, 0xc0012600u, 0x00000000u, 0x00000000u, 0xc0001300u, 0xffffffffu, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 13> Words8{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200001u, 0x00000000u, 0xc0039f00u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words9{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words10{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words11{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words12{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words13{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce202000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 12> Words14{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200200u, 0x00000000u, 0xc0024600u, 0x00000138u, 0x07fffc00u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 16> Words15{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200020u, 0x00000000u, 0xc0021102u, 0x00000001u, 0x00000000u, 0x00000000u, 0xc0021100u, 0x00000001u, 0x00000000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 13> Words16{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200002u, 0x00000000u, 0xc0036300u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words17{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words18{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words19{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words20{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words21{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce204000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 20> Words22{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200040u, 0x00000000u, 0xc0065800u, 0x86007fc0u, 0xfffffffeu, 0x000000ffu, 0x00000001u, 0x00000000u, 0x00000019u, 0x0000c3e1u, 0xc0004600u, 0x0000002eu, 0xc0004600u, 0x0000002cu, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 13> Words23{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200004u, 0x00000000u, 0xc0036400u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words24{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words25{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words26{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 8> Words27{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 13> Words28{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200800u, 0x00000000u, 0xc0036400u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 10> Words29{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200080u, 0x00000000u, 0xc0004600u, 0x00000407u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<std::uint32_t, 10> Words30{0xffff1000u, 0xc0027904u, 0x00000342u, 0xce200008u, 0x00000000u, 0xc0002f00u, 0x00000001u, 0xc0017904u, 0x00000342u, 0xcea00000u};
inline constexpr std::array<Case, 31> Cases{{{0x00000000u, Words0}, {0x10000000u, Words1}, {0x01000000u, Words2}, {0x00100000u, Words3}, {0x00010000u, Words4}, {0x00001000u, Words5}, {0x00000100u, Words6}, {0x00000010u, Words7}, {0x00000001u, Words8}, {0x20000000u, Words9}, {0x02000000u, Words10}, {0x00200000u, Words11}, {0x00020000u, Words12}, {0x00002000u, Words13}, {0x00000200u, Words14}, {0x00000020u, Words15}, {0x00000002u, Words16}, {0x40000000u, Words17}, {0x04000000u, Words18}, {0x00400000u, Words19}, {0x00040000u, Words20}, {0x00004000u, Words21}, {0x00000040u, Words22}, {0x00000004u, Words23}, {0x80000000u, Words24}, {0x08000000u, Words25}, {0x00800000u, Words26}, {0x00080000u, Words27}, {0x00000800u, Words28}, {0x00000080u, Words29}, {0x00000008u, Words30}}};
}
#endif
