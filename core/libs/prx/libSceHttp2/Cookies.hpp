#pragma once

#include <optional>
#include <string>
#include <string_view>

void Http2AddCookieBox(int cookieBoxId);
void Http2ForgetCookies(int cookieBoxId);
void Http2FlushCookies(int cookieBoxId);
std::optional<std::string> Http2CookieHeader(int cookieBoxId, std::string_view url);
void Http2StoreResponseCookie(int cookieBoxId, std::string_view url, std::string_view cookie);
