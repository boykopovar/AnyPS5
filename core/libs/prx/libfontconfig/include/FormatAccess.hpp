#pragma once

#include "prx/libfontconfig/include/Types.hpp"
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string> FontconfigLanguages_nid_no_patch(const FcLangSet* set, bool enumerate = false);
std::string FontconfigDowncase_nid_no_patch(std::string_view text);
