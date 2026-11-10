#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "curl form test failed at line %u\n", location.line());
    std::abort();
}

static guest_curl_slist* GuestNode(const char* text, guest_curl_slist* next) {
    auto* node = static_cast<guest_curl_slist*>(malloc_nid_postfix(sizeof(guest_curl_slist)));
    Require(node != nullptr);
    node->data = strdup_nid_postfix(text);
    node->next = next;
    return node;
}

static void FreeGuestForms(guest_curl_httppost* form) {
    while (form) {
        auto* const next = form->next;
        if (!(form->flags & CURL_HTTPPOST_PTRNAME)) free_nid_postfix(form->name);
        if (!(form->flags & (CURL_HTTPPOST_PTRCONTENTS | CURL_HTTPPOST_BUFFER | CURL_HTTPPOST_CALLBACK)))
            free_nid_postfix(form->contents);
        free_nid_postfix(form->contenttype);
        free_nid_postfix(form->showfilename);
        free_nid_postfix(form);
        form = next;
    }
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    guest_curl_httppost* form = nullptr;
    guest_curl_httppost* last = nullptr;
    auto* firstName = strdup_nid_postfix("first");
    auto* firstContents = strdup_nid_postfix("one");
    Require(curl_formadd_nid_postfix(&form, &last, CURLFORM_COPYNAME, firstName,
        CURLFORM_COPYCONTENTS, firstContents, CURLFORM_END) == CURL_FORMADD_OK);
    free_nid_postfix(firstName);
    free_nid_postfix(firstContents);
    Require(form != nullptr && last == form && std::strcmp(form->name, "first") == 0 &&
        std::strcmp(form->contents, "one") == 0);
    auto* secondName = strdup_nid_postfix("second");
    auto* secondContents = strdup_nid_postfix("two");
    Require(curl_formadd_nid_postfix(&form, &last, CURLFORM_COPYNAME, secondName,
        CURLFORM_COPYCONTENTS, secondContents, CURLFORM_END) == CURL_FORMADD_OK);
    free_nid_postfix(secondName);
    free_nid_postfix(secondContents);
    Require(form->next == last && last != form && std::strcmp(last->name, "second") == 0 &&
        std::strcmp(last->contents, "two") == 0);
    const guest_curl_forms arrayOptions[] = {
        {CURLFORM_COPYNAME, strdup_nid_postfix("array")},
        {CURLFORM_COPYCONTENTS, strdup_nid_postfix("three")},
        {CURLFORM_END, nullptr},
    };
    Require(curl_formadd_nid_postfix(&form, &last, CURLFORM_ARRAY, arrayOptions, CURLFORM_END) == CURL_FORMADD_OK);
    free_nid_postfix(const_cast<char*>(arrayOptions[0].value));
    free_nid_postfix(const_cast<char*>(arrayOptions[1].value));
    Require(form->next->next == last && last->next == nullptr && std::strcmp(last->name, "array") == 0 &&
        std::strcmp(last->contents, "three") == 0);
    const char binaryName[] = {'b', 'i', 'n', 'X'};
    const char binaryContents[] = {'a', '\0', 'b'};
    auto* const previous = last;
    Require(curl_formadd_nid_postfix(&form, &last, CURLFORM_COPYNAME, binaryName, CURLFORM_NAMELENGTH, std::int64_t{3},
        CURLFORM_COPYCONTENTS, binaryContents, CURLFORM_CONTENTSLENGTH, std::int64_t{3}, CURLFORM_END) == CURL_FORMADD_OK);
    Require(previous->next == last && last != previous && last->namelength == 3 && last->contentlen == 3 &&
        (last->flags & CURL_HTTPPOST_LARGE) && last->name != binaryName && last->contents != binaryContents &&
        std::memcmp(last->name, "bin", 3) == 0 && std::memcmp(last->contents, binaryContents, 3) == 0);
    auto* const tail = last;
    Require(curl_formadd_nid_postfix(&form, &last, CURLFORM_COPYNAME, "named", CURLFORM_COPYCONTENTS, "value",
        CURLFORM_ARRAY, static_cast<const guest_curl_forms*>(nullptr), CURLFORM_END) == CURL_FORMADD_NULL);
    Require(last == tail && tail->next == nullptr);
    Require(curl_formadd_nid_postfix(&form, &last, CURLFORM_COPYNAME, "named", CURLFORM_COPYNAME, "again",
        CURLFORM_ARRAY, static_cast<const guest_curl_forms*>(nullptr), CURLFORM_END) == CURL_FORMADD_OPTION_TWICE);
    Require(last == tail && tail->next == nullptr);
    FreeGuestForms(form);
    CURL* easy = curl_easy_init_nid_postfix();
    Require(easy != nullptr);
    curl_mime* mime = curl_mime_init_nid_postfix(easy);
    Require(mime != nullptr);
    curl_mimepart* part = curl_mime_addpart_nid_postfix(mime);
    Require(part != nullptr);
    Require(curl_mime_name_nid_postfix(part, "field") == CURLE_OK);
    const char bytes[] = {'a', '\0', 'b'};
    Require(curl_mime_data_nid_postfix(part, bytes, sizeof(bytes)) == CURLE_OK);
    Require(curl_mime_type_nid_postfix(part, "application/octet-stream") == CURLE_OK);
    Require(curl_mime_filename_nid_postfix(part, "upload.bin") == CURLE_OK);
    for (const char* encoder : {"binary", "8bit", "7bit", "base64", "quoted-printable"})
        Require(curl_mime_encoder_nid_postfix(part, encoder) == CURLE_OK);
    Require(curl_mime_encoder_nid_postfix(part, "unknown") == CURLE_BAD_FUNCTION_ARGUMENT);
    auto* owned = GuestNode("X-Mime: yes", GuestNode("X-Second: no", nullptr));
    Require(owned != nullptr);
    Require(curl_mime_headers_nid_postfix(part, owned, 1) == CURLE_OK);
    Require(curl_mime_headers_nid_postfix(part, nullptr, 0) == CURLE_OK);
    auto* sub = curl_mime_init_nid_postfix(easy);
    Require(sub != nullptr);
    auto* subpart = curl_mime_addpart_nid_postfix(sub);
    Require(subpart != nullptr);
    Require(curl_mime_data_nid_postfix(subpart, "nested", CURL_ZERO_TERMINATED) == CURLE_OK);
    auto* container = curl_mime_addpart_nid_postfix(mime);
    Require(container != nullptr);
    Require(curl_mime_subparts_nid_postfix(container, sub) == CURLE_OK);
    const char* filename = "curl-mime-test.tmp";
    auto* file = std::fopen(filename, "wb");
    Require(file != nullptr);
    Require(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    Require(std::fclose(file) == 0);
    Require(curl_mime_filedata_nid_postfix(part, filename) == CURLE_OK);
    Require(curl_mime_filedata_nid_postfix(part, nullptr) == CURLE_OK);
    Require(std::remove(filename) == 0);
    Require(curl_mime_data_nid_postfix(nullptr, "x", 1) == CURLE_BAD_FUNCTION_ARGUMENT);
    Require(curl_mime_subparts_nid_postfix(nullptr, nullptr) == CURLE_BAD_FUNCTION_ARGUMENT);
    curl_mime_free_nid_postfix(mime);
    curl_mime_free_nid_postfix(nullptr);
    curl_easy_cleanup_nid_postfix(easy);
    curl_global_cleanup_nid_postfix();
}
