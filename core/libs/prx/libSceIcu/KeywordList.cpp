#include "prx/libSceIcu/HostEnumeration.hpp"
#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "unicode/uloc.h"
#include "ulocimp.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

extern "C" {

UEnumeration* APS5_VABI uloc_openKeywordList_nid_postfix(const char* keywordList, std::int32_t keywordListSize,
                                                         std::int32_t* status) {
    return Icu::OpenHostEnumeration(
        status,
        [keywordList, keywordListSize](UErrorCode* local) -> UEnumeration* {
            if (keywordListSize < 0 || keywordListSize == std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("uloc_openKeywordList: invalid keyword list size");
            std::vector<char> keywords(static_cast<std::size_t>(keywordListSize));
            if (!keywords.empty()) {
                const bool copied = GuestCopyIn_nid_no_patch(keywords.data(), keywordList, keywords.size());
                if (!copied) throw std::invalid_argument("uloc_openKeywordList: unreadable keyword list");
                if (keywords.back() != '\0')
                    throw std::invalid_argument("uloc_openKeywordList: keyword list does not end with a NUL");
            }
            return uloc_openKeywordList(keywords.data(), keywordListSize, local);
        },
        __func__);
}

}
