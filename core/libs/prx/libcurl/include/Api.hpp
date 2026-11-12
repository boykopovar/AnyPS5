#pragma once

#include "SceTypes.hpp"
#include <cstddef>

struct CurlSlist {
    char* data;
    CurlSlist* next;
};

static_assert(sizeof(CurlSlist) == 16);
static_assert(offsetof(CurlSlist, data) == 0);
static_assert(offsetof(CurlSlist, next) == 8);

extern "C" {

CurlSlist* APS5_VABI curl_slist_append(CurlSlist* list, const char* string);
void APS5_VABI curl_slist_free_all(CurlSlist* list);
void APS5_VABI curl_free(void* pointer);
char* APS5_VABI curl_easy_escape(void* handle, const char* string, int length);
char* APS5_VABI curl_easy_unescape(void* handle, const char* string, int length, int* outputLength);
char* APS5_VABI curl_escape(const char* string, int length);
char* APS5_VABI curl_unescape(const char* string, int length);

}
