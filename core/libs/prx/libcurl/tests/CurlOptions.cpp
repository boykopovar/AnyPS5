#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool OptionFieldsEqual(const guest_curl_easyoption* lhs, const guest_curl_easyoption* rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }
    return std::strcmp(lhs->name, rhs->name) == 0 &&
           lhs->id == rhs->id &&
           lhs->type == rhs->type &&
           lhs->flags == rhs->flags;
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    const auto* opt_url = curl_easy_option_by_name_nid_postfix("URL");
    Require(opt_url != nullptr);
    Require(std::strcmp(opt_url->name, "URL") == 0);
    Require(opt_url->id == CURLOPT_URL);
    Require(opt_url->type == CURLOT_STRING);

    const auto* opt_url2 = curl_easy_option_by_name_nid_postfix("CURLOPT_URL");
    Require(opt_url2 == nullptr);

    const auto* opt_none = curl_easy_option_by_name_nid_postfix("TOTALLY_UNKNOWN_OPTION");
    Require(opt_none == nullptr);

    const auto* opt_by_id = curl_easy_option_by_id_nid_postfix(CURLOPT_URL);
    Require(opt_by_id != nullptr);
    Require(opt_by_id->id == CURLOPT_URL);

    const auto* opt_write_data = curl_easy_option_by_id_nid_postfix(CURLOPT_WRITEDATA);
    Require(opt_write_data != nullptr);
    Require(std::strcmp(opt_write_data->name, "WRITEDATA") == 0);
    Require((opt_write_data->flags & CURLOT_FLAG_ALIAS) == 0);

    const auto* opt_invalid_id = curl_easy_option_by_id_nid_postfix(999999);
    Require(opt_invalid_id == nullptr);

    const auto* optAcceptEncoding = curl_easy_option_by_name_nid_postfix("ACCEPT_ENCODING");
    Require(optAcceptEncoding != nullptr);
    Require(std::strcmp(optAcceptEncoding->name, "ACCEPT_ENCODING") == 0);
    Require(optAcceptEncoding->id == CURLOPT_ACCEPT_ENCODING);
    Require(optAcceptEncoding->type == CURLOT_STRING);
    Require((optAcceptEncoding->flags & CURLOT_FLAG_ALIAS) == 0);

    const auto* optEncoding = curl_easy_option_by_name_nid_postfix("ENCODING");
    Require(optEncoding != nullptr);
    Require(std::strcmp(optEncoding->name, "ENCODING") == 0);
    Require(optEncoding->id == optAcceptEncoding->id);
    Require(optEncoding->type == CURLOT_STRING);
    Require((optEncoding->flags & CURLOT_FLAG_ALIAS) != 0);

    const auto* optCanonicalById = curl_easy_option_by_id_nid_postfix(optEncoding->id);
    Require(OptionFieldsEqual(optCanonicalById, optAcceptEncoding));
    Require((optCanonicalById->flags & CURLOT_FLAG_ALIAS) == 0);

    const auto* optMixedAcceptEncoding = curl_easy_option_by_name_nid_postfix("AcCePt_EnCoDiNg");
    Require(OptionFieldsEqual(optMixedAcceptEncoding, optAcceptEncoding));

    const auto* optMixedEncoding = curl_easy_option_by_name_nid_postfix("EnCoDiNg");
    Require(OptionFieldsEqual(optMixedEncoding, optEncoding));

    int count = 0;
    int acceptEncodingCount = 0;
    int encodingCount = 0;
    const auto* current = curl_easy_option_next_nid_postfix(nullptr);
    Require(current != nullptr);
    while (current != nullptr) {
        Require(current->name != nullptr);
        Require(std::strlen(current->name) > 0);
        if (std::strcmp(current->name, "ACCEPT_ENCODING") == 0) {
            Require(OptionFieldsEqual(current, optAcceptEncoding));
            ++acceptEncodingCount;
        } else if (std::strcmp(current->name, "ENCODING") == 0) {
            Require(OptionFieldsEqual(current, optEncoding));
            ++encodingCount;
        }
        ++count;
        current = curl_easy_option_next_nid_postfix(current);
    }
    Require(count > 100);
    Require(acceptEncodingCount == 1);
    Require(encodingCount == 1);

    return 0;
}
