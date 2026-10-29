#include "prx/libSceSystemService/HostUrl.hpp"
#include <cstdlib>
#include <string>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void CheckValidator() {
    Require(ValidateHostUrl("http://example.com") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com/") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com:8443/a/b?c=d&e=f#g") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://[2001:db8::1]:443/x") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://[::1]/") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("http://a_b.example-site.com?q=%41%20") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com#frag") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("") == HostUrlVerdict::Empty);
}

void CheckScheme() {
    Require(ValidateHostUrl("http:") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("https:") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("http:example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("psno://example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("file:///etc/passwd") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("javascript:alert(1)") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("ftp://example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("HTTP://example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("//example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("example.com") == HostUrlVerdict::BadScheme);
}

void CheckCharacters() {
    Require(ValidateHostUrl("https://example.com/a b") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\n") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\t") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl(std::string("https://example.com/\0x", 22)) == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\x7f") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\x1b[0m") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\xc3\xa9") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://\xd0\xb0pple.com/") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\\x") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\"x") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/<x>") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/`x`") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/|x") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/{x}") == HostUrlVerdict::BadCharacter);
}

void CheckEscapes() {
    Require(ValidateHostUrl("https://example.com/%") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%4") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%zz") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%00") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%0a") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%0D") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%1F") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%7f") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%20") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com/%7E") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com/%c3%a9") == HostUrlVerdict::Ok);
}

void CheckAuthority() {
    Require(ValidateHostUrl("http://") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https:///path") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://?q=1") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://#x") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://:443/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://exa mple.com/") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://exam!ple.com/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://exam%41ple.com/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[]/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[::g]/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[::1/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[::1]x/") == HostUrlVerdict::BadAuthority);
}

void CheckCredentials() {
    Require(ValidateHostUrl("https://user@example.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://user:secret@example.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://@example.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://good.com@evil.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://good.com:443@evil.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://example.com/path@x") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com?u=a@b") == HostUrlVerdict::Ok);
}

void CheckPort() {
    Require(ValidateHostUrl("https://example.com:/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:0/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:65536/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:123456/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:80a/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:65535/") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com:1") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://[::1]:/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://[::1]:99999/") == HostUrlVerdict::BadPort);
}

void CheckLength() {
    const std::string prefix = "https://example.com/";
    Require(ValidateHostUrl(prefix + std::string(HOST_URL_MAX_LENGTH - prefix.size(), 'a')) == HostUrlVerdict::Ok);
    Require(ValidateHostUrl(prefix + std::string(HOST_URL_MAX_LENGTH - prefix.size() + 1, 'a')) == HostUrlVerdict::TooLong);
    Require(ValidateHostUrl(std::string(100000, 'a')) == HostUrlVerdict::TooLong);
}

}

int main() {
    CheckValidator();
    CheckScheme();
    CheckCharacters();
    CheckEscapes();
    CheckAuthority();
    CheckCredentials();
    CheckPort();
    CheckLength();
    return 0;
}
