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

    const auto* opt_invalid_id = curl_easy_option_by_id_nid_postfix(static_cast<CURLoption>(999999));
    Require(opt_invalid_id == nullptr);

    int count = 0;
    const auto* current = curl_easy_option_next_nid_postfix(nullptr);
    Require(current != nullptr);
    while (current != nullptr) {
        Require(current->name != nullptr);
        Require(std::strlen(current->name) > 0);
        ++count;
        current = curl_easy_option_next_nid_postfix(current);
    }
    Require(count > 100);

    return 0;
}
