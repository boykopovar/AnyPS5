#ifndef CORE_LIBS_SCE_NP_TROPHY2_TYPES_HPP
#define CORE_LIBS_SCE_NP_TROPHY2_TYPES_HPP

#include <cstddef>
#include <cstdint>

struct NpTrophy2Progress {
    std::int32_t type;
    std::uint8_t reserved[4];
    std::uint64_t value;
};

struct NpTrophy2GameDetails {
    std::uint32_t num_groups;
    std::uint32_t num_trophies;
    std::uint32_t num_platinum;
    std::uint32_t num_gold;
    std::uint32_t num_silver;
    std::uint32_t num_bronze;
    char title[128];
};

struct NpTrophy2GameData {
    std::uint32_t unlocked_trophies;
    std::uint32_t unlocked_platinum;
    std::uint32_t unlocked_gold;
    std::uint32_t unlocked_silver;
    std::uint32_t unlocked_bronze;
    std::uint32_t progress_percentage;
};

struct NpTrophy2GroupDetails {
    std::int32_t group_id;
    std::uint32_t num_trophies;
    std::uint32_t num_platinum;
    std::uint32_t num_gold;
    std::uint32_t num_silver;
    std::uint32_t num_bronze;
    char title[128];
};

struct NpTrophy2GroupData {
    std::int32_t group_id;
    std::uint32_t unlocked_trophies;
    std::uint32_t unlocked_platinum;
    std::uint32_t unlocked_gold;
    std::uint32_t unlocked_silver;
    std::uint32_t unlocked_bronze;
    std::uint32_t progress_percentage;
    std::uint8_t reserved[4];
};

struct NpTrophy2Details {
    std::int32_t trophy_id;
    std::int32_t trophy_grade;
    std::int32_t group_id;
    bool hidden;
    bool has_reward;
    std::uint8_t reserved2[2];
    NpTrophy2Progress target;
    char name[128];
    char description[1024];
    char reward[128];
};

struct NpTrophy2Data {
    std::int32_t trophy_id;
    bool unlocked;
    std::uint8_t reserved[3];
    NpTrophy2Progress progress;
    std::uint64_t timestamp_tick;
};

static_assert(sizeof(NpTrophy2Progress) == 16);
static_assert(sizeof(NpTrophy2GameDetails) == 152);
static_assert(sizeof(NpTrophy2GameData) == 24);
static_assert(sizeof(NpTrophy2GroupDetails) == 152);
static_assert(sizeof(NpTrophy2GroupData) == 32);
static_assert(sizeof(NpTrophy2Details) == 1312);
static_assert(sizeof(NpTrophy2Data) == 32);

#endif
