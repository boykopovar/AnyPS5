#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "MimeTypeRegistry test failed at line %d\n", line);
        std::abort();
    }
}

#define Require(condition) Check((condition), __LINE__)

using String = NKWebKit::WtfAbi::String;
using StringView = NKWebKit::WtfAbi::StringView;
template <typename T>
using Vector = NKWebKit::WtfAbi::Vector<T>;
template <typename T>
using FixedVector = NKWebKit::WtfAbi::FixedVector<T>;
using ASCIILiteral = NKWebKit::WtfAbi::ASCIILiteral;

String MakeString(const char* s) {
    if (!s) {
        return String{};
    }
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(s));
}

StringView MakeView(const char* s) {
    if (!s) {
        return StringView{nullptr, 0, true};
    }
    return StringView{s, static_cast<unsigned>(std::strlen(s)), true};
}

bool Equals(const String& s, const char* expected) {
    if (!s.impl) {
        return expected == nullptr;
    }
    if (!expected) {
        return false;
    }
    std::size_t len = std::strlen(expected);
    if (s.impl->length != len) {
        return false;
    }
    bool is8Bit = (s.impl->hashAndFlags & 4) != 0;
    if (is8Bit) {
        return std::memcmp(s.impl->data, expected, len) == 0;
    }
    const auto* p16 = static_cast<const char16_t*>(s.impl->data);
    for (std::size_t i = 0; i < len; ++i) {
        if (p16[i] != static_cast<char16_t>(expected[i])) {
            return false;
        }
    }
    return true;
}

bool Contains(const Vector<String>& vec, const char* str) {
    for (unsigned i = 0; i < vec.size; ++i) {
        if (Equals(vec[i], str)) {
            return true;
        }
    }
    return false;
}

void TestFixedVectorLists() {
    auto pdfTypes = _ZN7WebCore16MIMETypeRegistry12pdfMIMETypesEv();
    Require(pdfTypes.size() == 2);
    Require(std::strcmp(pdfTypes[0].characters, "application/pdf") == 0);
    Require(std::strcmp(pdfTypes[1].characters, "text/pdf") == 0);

    auto imgTypes = _ZN7WebCore16MIMETypeRegistry23supportedImageMIMETypesEv();
    Require(imgTypes.size() == 10);
    Require(std::strcmp(imgTypes[0].characters, "image/apng") == 0);
    Require(std::strcmp(imgTypes[1].characters, "image/bmp") == 0);
    Require(std::strcmp(imgTypes[2].characters, "image/gif") == 0);
    Require(std::strcmp(imgTypes[3].characters, "image/jpeg") == 0);
    Require(std::strcmp(imgTypes[4].characters, "image/jpg") == 0);
    Require(std::strcmp(imgTypes[5].characters, "image/png") == 0);
    Require(std::strcmp(imgTypes[6].characters, "image/vnd.microsoft.icon") == 0);
    Require(std::strcmp(imgTypes[7].characters, "image/webp") == 0);
    Require(std::strcmp(imgTypes[8].characters, "image/x-icon") == 0);
    Require(std::strcmp(imgTypes[9].characters, "image/x-xbitmap") == 0);

    auto unsuppText = _ZN7WebCore16MIMETypeRegistry24unsupportedTextMIMETypesEv();
    Require(unsuppText.size() == 13);
    Require(std::strcmp(unsuppText[0].characters, "text/calendar") == 0);
    Require(std::strcmp(unsuppText[4].characters, "text/rtf") == 0);
    Require(std::strcmp(unsuppText[12].characters, "text/x-vcf") == 0);
}

void TestTypePredicates() {
    Require(_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("text/xml")));
    Require(_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("TEXT/XML")));
    Require(_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("application/xml")));
    Require(_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("image/svg+xml")));
    Require(_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("application/xhtml+xml")));
    Require(!_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("text/html")));
    Require(!_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("image/png")));
    Require(!_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString(nullptr)));
    Require(!_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("+xml")));
    Require(!_ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(MakeString("a/+xml")));

    Require(!_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("text/html")));
    Require(!_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("text/xml")));
    Require(!_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("text/xsl")));
    Require(_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("text/plain")));
    Require(_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("text/css")));
    Require(_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("application/json")));
    Require(_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("text/json")));
    Require(_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("application/javascript")));
    Require(!_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("image/png")));
    Require(!_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(MakeString(nullptr)));

    Require(!_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView("text/xml")));
    Require(!_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView("application/xml")));
    Require(_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView("text/xml-external-parsed-entity")));
    Require(_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView("TEXT/XML-EXTERNAL-PARSED-ENTITY")));
    Require(_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView("application/xml-external-parsed-entity")));
    Require(!_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView("text/html")));
    Require(!_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView("")));
    Require(!_ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(MakeView(nullptr)));

    Require(_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("application/json")));
    Require(_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("APPLICATION/JSON")));
    Require(_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("text/json")));
    Require(_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("application/problem+json")));
    Require(!_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("application/javascript")));
    Require(!_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("image/png")));
    Require(!_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("+json")));
    Require(!_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(MakeString(nullptr)));

    Require(_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString("text/javascript")));
    Require(_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString("TEXT/JAVASCRIPT")));
    Require(_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString("application/javascript")));
    Require(_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString("application/x-javascript")));
    Require(_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString("text/livescript")));
    Require(!_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString("application/json")));
    Require(!_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(MakeString(nullptr)));

    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/apng")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("IMAGE/APNG")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/png")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/jpeg")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/gif")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/webp")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/x-icon")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/vnd.microsoft.icon")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("application/ico")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("text/ico")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/jpg")));
    Require(_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("image/x-ms-bmp")));
    Require(!_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("text/html")));
    Require(!_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("video/mp4")));
    Require(!_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(MakeString(nullptr)));

    Require(_ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(MakeString("application/pdf")));
    Require(_ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(MakeString("APPLICATION/PDF")));
    Require(_ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(MakeString("text/pdf")));
    Require(_ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(MakeString("application/postscript")));
    Require(!_ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(MakeString("image/png")));
    Require(!_ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(MakeString(nullptr)));

    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("text/html")));
    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("TEXT/HTML")));
    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("text/plain")));
    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("application/xhtml+xml")));
    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("image/svg+xml")));
    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("application/json")));
    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("application/javascript")));
    Require(_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("multipart/x-mixed-replace")));
    Require(!_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("application/pdf")));
    Require(!_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("image/png")));
    Require(!_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(MakeString(nullptr)));

    Require(_ZN7WebCore16MIMETypeRegistry28isSupportedTextTrackMIMETypeERKN3WTF6StringE(MakeString("text/vtt")));
    Require(_ZN7WebCore16MIMETypeRegistry28isSupportedTextTrackMIMETypeERKN3WTF6StringE(MakeString("TEXT/VTT")));
    Require(!_ZN7WebCore16MIMETypeRegistry28isSupportedTextTrackMIMETypeERKN3WTF6StringE(MakeString("text/html")));
    Require(!_ZN7WebCore16MIMETypeRegistry28isSupportedTextTrackMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry28isSupportedTextTrackMIMETypeERKN3WTF6StringE(MakeString(nullptr)));

    Require(_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString("image/apng")));
    Require(_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString("image/png")));
    Require(_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString("image/svg+xml")));
    Require(_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString("IMAGE/SVG+XML")));
    Require(_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString("image/webp")));
    Require(!_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString("text/plain")));
    Require(!_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(MakeString(nullptr)));
}

void TestExtensions() {
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("image/png")), "png"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("IMAGE/PNG")), "png"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("image/jpeg")), "jpeg"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("image/webp")), "webp"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("text/html")), "htm"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("text/plain")), "text"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("video/mp4")), "mp4"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("audio/mp4")), ""));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("video/webm")), ""));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("application/pdf")), "pdf"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("application/x-javascript")), "js"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString("unknown/type")), ""));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(MakeString(nullptr)), ""));

    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString("test"), MakeString("text/html")), "test.htm"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString("test.html"), MakeString("text/html")), "test.html"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString("test.png"), MakeString("text/html")), "test.png"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString("test"), MakeString("application/octet-stream")), "test"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString("test"), MakeString("unknown/type")), "test"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString("test"), MakeString("image/png")), "test.png"));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString(""), MakeString("image/png")), ""));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
        MakeString(nullptr), MakeString("image/png")), nullptr));
}

void TestAllowedAndEncoding() {
    Vector<String> emptyMimes;
    Vector<String> emptyExts;

    auto allMimes = _ZN7WebCore16MIMETypeRegistry16allowedMIMETypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(emptyMimes, emptyExts);
    Require(allMimes.size == 0);
    Require(!_ZN7WebCore16MIMETypeRegistry32containsImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(emptyMimes, emptyExts));
    Require(_ZN7WebCore16MIMETypeRegistry33preferredImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(emptyMimes, emptyExts).impl == nullptr);

    Vector<String> sampleExts;
    sampleExts.append(MakeString("png"));
    sampleExts.append(MakeString(".html"));
    sampleExts.append(MakeString("unknown_ext"));
    auto convertedMimes = _ZN7WebCore16MIMETypeRegistry16allowedMIMETypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(emptyMimes, sampleExts);
    Require(convertedMimes.size == 2);
    Require(Contains(convertedMimes, "image/png"));
    Require(Contains(convertedMimes, "text/html"));
    Require(_ZN7WebCore16MIMETypeRegistry32containsImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(emptyMimes, sampleExts));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry33preferredImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(emptyMimes, sampleExts), "image/png"));

    Vector<String> sampleMimes;
    sampleMimes.append(MakeString("IMAGE/PNG"));
    sampleMimes.append(MakeString("image/jpeg"));
    auto combinedMimes = _ZN7WebCore16MIMETypeRegistry16allowedMIMETypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(sampleMimes, emptyExts);
    Require(combinedMimes.size == 2);
    Require(Contains(combinedMimes, "image/png"));
    Require(Contains(combinedMimes, "image/jpeg"));
    Require(_ZN7WebCore16MIMETypeRegistry32containsImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(sampleMimes, emptyExts));
    Require(Equals(_ZN7WebCore16MIMETypeRegistry33preferredImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(sampleMimes, emptyExts), "image/png"));

    Vector<String> nonPngMimes;
    nonPngMimes.append(MakeString("image/jpeg"));
    Require(!_ZN7WebCore16MIMETypeRegistry32containsImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(nonPngMimes, emptyExts));
    Require(_ZN7WebCore16MIMETypeRegistry33preferredImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(nonPngMimes, emptyExts).impl == nullptr);
}

void TestLegacySchemeRegistry() {
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("http")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("HTTP")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("https")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("ftp")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("file")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("data")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("about")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("blob")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("ws")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("wss")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("javascript")));
    Require(_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("webkit-fake-url")));
    Require(!_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("unknown-scheme")));
    Require(!_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(MakeString(nullptr)));

    _ZN7WebCore20LegacySchemeRegistry24registerURLSchemeAsLocalERKN3WTF6StringE(MakeString("custom-local"));
    _ZN7WebCore20LegacySchemeRegistry24registerURLSchemeAsLocalERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry25registerURLSchemeAsSecureERKN3WTF6StringE(MakeString("custom-secure"));
    _ZN7WebCore20LegacySchemeRegistry25registerURLSchemeAsSecureERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry27registerURLSchemeAsNoAccessERKN3WTF6StringE(MakeString("custom-noaccess"));
    _ZN7WebCore20LegacySchemeRegistry27registerURLSchemeAsNoAccessERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry32registerURLSchemeAsEmptyDocumentERKN3WTF6StringE(MakeString("custom-emptydoc"));
    _ZN7WebCore20LegacySchemeRegistry32registerURLSchemeAsEmptyDocumentERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry34registerURLSchemeAsDisplayIsolatedERKN3WTF6StringE(MakeString("custom-isolated"));
    _ZN7WebCore20LegacySchemeRegistry34registerURLSchemeAsDisplayIsolatedERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry35registerURLSchemeAsCachePartitionedERKN3WTF6StringE(MakeString("custom-cachepart"));
    _ZN7WebCore20LegacySchemeRegistry35registerURLSchemeAsCachePartitionedERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry36registerAsCanDisplayOnlyIfCanRequestERKN3WTF6StringE(MakeString("custom-candisplay"));
    _ZN7WebCore20LegacySchemeRegistry36registerAsCanDisplayOnlyIfCanRequestERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry36registerURLSchemeAsAlwaysRevalidatedERKN3WTF6StringE(MakeString("custom-reval"));
    _ZN7WebCore20LegacySchemeRegistry36registerURLSchemeAsAlwaysRevalidatedERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry40setDomainRelaxationForbiddenForURLSchemeEbRKN3WTF6StringE(true, MakeString("custom-domain"));
    _ZN7WebCore20LegacySchemeRegistry40setDomainRelaxationForbiddenForURLSchemeEbRKN3WTF6StringE(false, MakeString("custom-domain"));
    _ZN7WebCore20LegacySchemeRegistry40setDomainRelaxationForbiddenForURLSchemeEbRKN3WTF6StringE(true, MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry49registerURLSchemeAsBypassingContentSecurityPolicyERKN3WTF6StringE(MakeString("custom-csp"));
    _ZN7WebCore20LegacySchemeRegistry49registerURLSchemeAsBypassingContentSecurityPolicyERKN3WTF6StringE(MakeString(nullptr));
    _ZN7WebCore20LegacySchemeRegistry57removeURLSchemeRegisteredAsBypassingContentSecurityPolicyERKN3WTF6StringE(MakeString("custom-csp"));
    _ZN7WebCore20LegacySchemeRegistry57removeURLSchemeRegisteredAsBypassingContentSecurityPolicyERKN3WTF6StringE(MakeString(nullptr));

    _ZN7WebCore20LegacySchemeRegistry58registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsingERKN3WTF6StringE(MakeString("custom-privatedb"));
    _ZN7WebCore20LegacySchemeRegistry58registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsingERKN3WTF6StringE(MakeString(nullptr));

    Require(_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("http")));
    Require(_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("HTTP")));
    Require(_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("https")));
    Require(!_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("data")));
    Require(!_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("custom-cors")));
    Require(!_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("")));
    Require(!_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString(nullptr)));

    _ZN7WebCore20LegacySchemeRegistry30registerURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("custom-cors"));
    _ZN7WebCore20LegacySchemeRegistry30registerURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString(nullptr));
    Require(_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("custom-cors")));
    Require(_ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(MakeString("CUSTOM-CORS")));

    auto corsSchemes = _ZN7WebCore20LegacySchemeRegistry36allURLSchemesRegisteredAsCORSEnabledEv();
    Require(corsSchemes.size >= 3);
    Require(Contains(corsSchemes, "http"));
    Require(Contains(corsSchemes, "https"));
    Require(!Contains(corsSchemes, "data"));
    Require(Contains(corsSchemes, "custom-cors"));
}

}  // namespace

int main() {
    TestFixedVectorLists();
    TestTypePredicates();
    TestExtensions();
    TestAllowedAndEncoding();
    TestLegacySchemeRegistry();
    std::printf("All libSceNKWebKit MimeTypeRegistry tests passed!\n");
    return 0;
}