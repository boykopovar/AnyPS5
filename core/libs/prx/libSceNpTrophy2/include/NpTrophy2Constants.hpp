#ifndef CORE_LIBS_PRX_LIBSCENPTROPHY2_NPTROPHY2CONSTANTS_HPP
#define CORE_LIBS_PRX_LIBSCENPTROPHY2_NPTROPHY2CONSTANTS_HPP

#include <cstdint>

static constexpr int SCE_NP_TROPHY2_OK = 0;
static constexpr int SCE_NP_TROPHY2_ERROR_ICON_FILE_NOT_FOUND = -2141898479;

static constexpr int NP_TROPHY2_CONTEXT_DEFAULT = 1;
static constexpr int NP_TROPHY2_HANDLE_DEFAULT = 1;

static constexpr std::int32_t NP_TROPHY2_GROUP_ID_BASE = 0;
static constexpr std::int32_t NP_TROPHY2_TROPHY_ID_DEFAULT = 0;

static constexpr std::uint32_t NP_TROPHY2_NUM_GROUPS = 1;
static constexpr std::uint32_t NP_TROPHY2_NUM_TROPHIES = 1;
static constexpr std::uint32_t NP_TROPHY2_NUM_PLATINUM = 0;
static constexpr std::uint32_t NP_TROPHY2_NUM_GOLD = 0;
static constexpr std::uint32_t NP_TROPHY2_NUM_SILVER = 0;
static constexpr std::uint32_t NP_TROPHY2_NUM_BRONZE = 1;

static constexpr std::uint32_t NP_TROPHY2_UNLOCKED_TROPHIES = 0;
static constexpr std::uint32_t NP_TROPHY2_UNLOCKED_PLATINUM = 0;
static constexpr std::uint32_t NP_TROPHY2_UNLOCKED_GOLD = 0;
static constexpr std::uint32_t NP_TROPHY2_UNLOCKED_SILVER = 0;
static constexpr std::uint32_t NP_TROPHY2_UNLOCKED_BRONZE = 0;
static constexpr std::uint32_t NP_TROPHY2_PROGRESS_PERCENTAGE = 0;

static constexpr std::int32_t NP_TROPHY2_TROPHY_GRADE_BRONZE = 4;

static constexpr std::int32_t NP_TROPHY2_PROGRESS_TYPE_NONE = 0;
static constexpr std::uint32_t NP_TROPHY2_PROGRESS_VALUE_NONE = 0;

static constexpr std::size_t NP_TROPHY2_ICON_SIZE_NONE = 0;

static constexpr const char NP_TROPHY2_GAME_TITLE[] = "Game";
static constexpr const char NP_TROPHY2_GROUP_TITLE[] = "Base Game";
static constexpr const char NP_TROPHY2_TROPHY_NAME[] = "Trophy";
static constexpr const char NP_TROPHY2_TROPHY_DESCRIPTION[] = "Trophy";
static constexpr const char NP_TROPHY2_TROPHY_REWARD[] = "";

#endif
