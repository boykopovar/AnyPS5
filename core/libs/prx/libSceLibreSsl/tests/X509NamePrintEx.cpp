#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint64_t Esc2253 = 1;
constexpr std::uint64_t EscCtrl = 2;
constexpr std::uint64_t EscMsb = 4;
constexpr std::uint64_t EscQuote = 8;
constexpr std::uint64_t Utf8Convert = 0x10;
constexpr std::uint64_t IgnoreType = 0x20;
constexpr std::uint64_t ShowType = 0x40;
constexpr std::uint64_t DumpAll = 0x80;
constexpr std::uint64_t DumpUnknown = 0x100;
constexpr std::uint64_t DumpDer = 0x200;
constexpr std::uint64_t Rfc2253Strings = Esc2253 | EscCtrl | EscMsb | Utf8Convert | DumpUnknown | DumpDer;
constexpr std::uint64_t SepCommaPlus = 1ull << 16;
constexpr std::uint64_t SepCplusSpc = 2ull << 16;
constexpr std::uint64_t SepSplusSpc = 3ull << 16;
constexpr std::uint64_t SepMultiline = 4ull << 16;
constexpr std::uint64_t DnRev = 1ull << 20;
constexpr std::uint64_t FnLn = 1ull << 21;
constexpr std::uint64_t FnOid = 2ull << 21;
constexpr std::uint64_t FnNone = 3ull << 21;
constexpr std::uint64_t SpcEq = 1ull << 23;
constexpr std::uint64_t DumpUnknownFields = 1ull << 24;
constexpr std::uint64_t FnAlign = 1ull << 25;
constexpr std::uint64_t Rfc2253 = Rfc2253Strings | SepCommaPlus | DnRev | DumpUnknownFields;
constexpr std::uint64_t Oneline = Rfc2253Strings | EscQuote | SepCplusSpc | SpcEq;
constexpr std::uint64_t Multiline = EscCtrl | EscMsb | SepMultiline | SpcEq | FnLn | FnAlign;

constexpr int Country = 14;
constexpr int Organization = 17;
constexpr int CommonName = 13;
constexpr int OrganizationalUnit = 18;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

class Builder {
public:
    Builder& Add(const LibreSslAsn1Object* object, std::string bytes, int type = 12, bool sameSet = false) {
        values.push_back(std::move(bytes));
        types.push_back(type);
        sets.push_back(sameSet && !sets.empty() ? sets.back() : static_cast<int>(sets.size()));
        objects.push_back(object);
        return *this;
    }

    LibreSslX509Name* Build() {
        strings.assign(values.size(), Asn1String{});
        entries.assign(values.size(), LibreSslX509NameEntry{});
        pointers.clear();
        for (std::size_t index = 0; index < entries.size(); ++index) {
            strings[index] = Asn1String{static_cast<std::int32_t>(values[index].size()), types[index],
                reinterpret_cast<unsigned char*>(values[index].data()), 0};
            entries[index] = LibreSslX509NameEntry{const_cast<LibreSslAsn1Object*>(objects[index]), &strings[index], sets[index], 0};
            pointers.push_back(reinterpret_cast<char*>(&entries[index]));
        }
        stack = LibreSslStack{static_cast<std::int32_t>(entries.size()), pointers.data(), 0, static_cast<std::int32_t>(pointers.size()), nullptr};
        name = LibreSslX509Name{&stack, 0, nullptr, nullptr, 0};
        return &name;
    }

private:
    std::vector<std::string> values;
    std::vector<int> types;
    std::vector<int> sets;
    std::vector<const LibreSslAsn1Object*> objects;
    std::vector<Asn1String> strings;
    std::vector<LibreSslX509NameEntry> entries;
    std::vector<char*> pointers;
    LibreSslStack stack{};
    LibreSslX509Name name{};
};

const LibreSslAsn1Object* Object(int nid) {
    return &LibreSslObjects_nid_no_patch[nid];
}

struct Printed {
    std::string text;
    int result;
};

Printed Print(const LibreSslX509Name* name, int indent, std::uint64_t flags) {
    LibreSsl::Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr, "a memory BIO");
    const int result = X509_NAME_print_ex_nid_postfix(bio, name, indent, flags);
    char* data = nullptr;
    const auto length = BIO_ctrl_nid_postfix(bio, 3, 0, &data);
    Printed printed{std::string(data ? data : "", static_cast<std::size_t>(length)), result};
    BIO_free_nid_postfix(bio);
    return printed;
}

void Expect(const LibreSslX509Name* name, int indent, std::uint64_t flags, const std::string& expected, const char* message) {
    const Printed printed = Print(name, indent, flags);
    if (printed.text != expected || printed.result != static_cast<int>(expected.size())) {
        std::fprintf(stderr, "failed: %s\n  got %d \"%s\"\n  want %zu \"%s\"\n", message, printed.result, printed.text.c_str(), expected.size(), expected.c_str());
        std::abort();
    }
}

void ExpectFailure(const LibreSslX509Name* name, int indent, std::uint64_t flags, const std::string& written, const char* message) {
    const Printed printed = Print(name, indent, flags);
    if (printed.result != -1 || printed.text != written) {
        std::fprintf(stderr, "failed: %s\n  got %d \"%s\"\n", message, printed.result, printed.text.c_str());
        std::abort();
    }
}

void Presets() {
    Builder builder;
    builder.Add(Object(Country), "US").Add(Object(Organization), "Example, Inc.").Add(Object(CommonName), "a.example.com");
    const LibreSslX509Name* name = builder.Build();
    Expect(name, 0, Rfc2253, "CN=a.example.com,O=Example\\, Inc.,C=US", "RFC 2253 reverses the entries and escapes the comma");
    Expect(name, 0, Oneline, "C = US, O = \"Example, Inc.\", CN = a.example.com", "the one-line form quotes a value with a special character");
    Expect(name, 0, Multiline,
        "countryName" + std::string(14, ' ') + " = US\norganizationName" + std::string(9, ' ') + " = Example, Inc.\ncommonName" + std::string(15, ' ') + " = a.example.com",
        "the multi-line form pads long names to 25 columns");
    Expect(name, 4, Multiline,
        "    countryName" + std::string(14, ' ') + " = US\n    organizationName" + std::string(9, ' ') + " = Example, Inc.\n    commonName" + std::string(15, ' ') + " = a.example.com",
        "the multi-line form indents every line and counts the indentation");
    Expect(name, -3, SepCommaPlus, "C=US,O=Example, Inc.,CN=a.example.com", "a negative indent is zero");
    Expect(name, 5, SepSplusSpc | FnLn, "     countryName=US; organizationName=Example, Inc.; commonName=a.example.com", "the separator forms do not indent after the first entry");
    Expect(name, 0, SepCommaPlus | FnNone, "US,Example, Inc.,a.example.com", "no field names prints values only");
    Expect(name, 0, SepCommaPlus | SpcEq | FnLn | FnAlign, "countryName               = US,organizationName          = Example, Inc.,commonName                = a.example.com",
        "alignment pads the long names");
    Expect(name, 0, SepCommaPlus | FnAlign, "C         =US,O         =Example, Inc.,CN        =a.example.com", "alignment pads short names to ten columns");
}

void Compatibility() {
    Builder builder;
    builder.Add(Object(Country), "US").Add(Object(CommonName), "example.com");
    const LibreSslX509Name* name = builder.Build();
    const Printed printed = Print(name, 0, 0);
    Require(printed.result == 1 && printed.text == "C=US, CN=example.com", "flags of zero use the traditional comma form and return 1");
    Require(Print(name, 7, 0).text == "C=US, CN=example.com", "compatibility ignores the indent");
    Builder empty;
    Require(Print(empty.Build(), 0, 0).result == 1 && Print(empty.Build(), 0, 0).text.empty(), "an empty name prints nothing and succeeds");
    Require(Print(nullptr, 0, 0).text == "O X509_NAME", "a null name prints what X509_NAME_oneline returns without its first byte");
    Require(X509_NAME_print_ex_nid_postfix(nullptr, name, 0, 0) == 0, "compatibility with no BIO fails");
}

void Counting() {
    Builder builder;
    builder.Add(Object(Country), "US").Add(Object(Organization), "Example, Inc.").Add(Object(CommonName), "a.example.com");
    const LibreSslX509Name* name = builder.Build();
    Require(X509_NAME_print_ex_nid_postfix(nullptr, name, 0, Rfc2253) == 38, "no BIO only counts");
    Require(X509_NAME_print_ex_nid_postfix(nullptr, name, 3, Multiline) == 123, "indent is counted");
    Require(X509_NAME_print_ex_nid_postfix(nullptr, nullptr, 6, Rfc2253) == 6, "a null name yields only the indent");
    LibreSslX509Name noStack{nullptr, 0, nullptr, nullptr, 0};
    Require(X509_NAME_print_ex_nid_postfix(nullptr, &noStack, 2, Rfc2253) == 2, "a name without an entry stack yields only the indent");
}

void Separators() {
    Builder builder;
    builder.Add(Object(Country), "US").Add(Object(Organization), "A").Add(Object(OrganizationalUnit), "B", 12, true).Add(Object(CommonName), "C");
    const LibreSslX509Name* name = builder.Build();
    Expect(name, 0, SepCommaPlus, "C=US,O=A+OU=B,CN=C", "a shared RDN is joined with a plus");
    Expect(name, 0, SepCplusSpc, "C=US, O=A + OU=B, CN=C", "the spaced separators");
    Expect(name, 0, SepSplusSpc, "C=US; O=A + OU=B; CN=C", "the semicolon separators");
    Expect(name, 2, SepMultiline, "  C=US\n  O=A + OU=B\n  CN=C", "the multi-line separator indents each line");
    Expect(name, 0, SepCommaPlus | DnRev, "CN=C,OU=B+O=A,C=US", "reversal keeps the RDN grouping");
    ExpectFailure(name, 3, 5ull << 16, "   ", "an unknown separator class fails after the indent");
    ExpectFailure(name, 0, 0xfull << 16, "", "an unassigned separator class fails");
}

void Objects() {
    unsigned char unknownData[] = {0x2a, 0x03, 0x04};
    LibreSslAsn1Object unknown{nullptr, nullptr, 0, 3, unknownData, 0};
    Builder builder;
    builder.Add(&unknown, "x").Add(Object(CommonName), "y");
    const LibreSslX509Name* name = builder.Build();
    Expect(name, 0, SepCommaPlus, "1.2.3.4=x,CN=y", "an unknown object prints in dotted form");
    Expect(name, 0, SepCommaPlus | FnOid, "1.2.3.4=x,2.5.4.3=y", "OID names are numeric for every entry");
    Expect(name, 0, SepCommaPlus | FnLn | FnAlign | SpcEq, "1.2.3.4 = x,commonName                = y", "unknown objects are not aligned");
    Expect(name, 0, SepCommaPlus | DumpUnknownFields, "1.2.3.4=#78,CN=y", "an unknown field is dumped as content octets");
    Expect(name, 0, SepCommaPlus | DumpUnknownFields | DumpDer, "1.2.3.4=#0C0178,CN=y", "an unknown field is dumped as DER");
    Expect(name, 0, SepCommaPlus | DumpUnknownFields | DumpUnknown, "1.2.3.4=#78,CN=y", "the content dump does not need the unknown dump flag");
}

void Strings() {
    Builder leading;
    leading.Add(Object(CommonName), " a#b ");
    Expect(leading.Build(), 0, SepCommaPlus | Esc2253, "CN=\\ a#b\\ ", "leading and trailing spaces are escaped");
    Builder hash;
    hash.Add(Object(CommonName), "#ab");
    Expect(hash.Build(), 0, SepCommaPlus | Esc2253, "CN=\\#ab", "a leading hash is escaped");
    Builder single;
    single.Add(Object(CommonName), " ");
    Expect(single.Build(), 0, SepCommaPlus | Esc2253, "CN=\\ ", "a lone space is escaped as the last character");
    Builder special;
    special.Add(Object(CommonName), "a\"b\\c+d,e;f<g>h=i");
    Expect(special.Build(), 0, SepCommaPlus | Esc2253, "CN=a\\\"b\\\\c\\+d\\,e\\;f\\<g\\>h=i", "RFC 2253 special characters get a backslash");
    Expect(special.Build(), 0, SepCommaPlus | Esc2253 | EscQuote, "CN=\"a\\\"b\\\\c+d,e;f<g>h=i\"", "quoting wraps the value while quote and backslash stay escaped");
    Expect(special.Build(), 0, SepCommaPlus, "CN=a\"b\\c+d,e;f<g>h=i", "no escape flags print the value as is");
    Builder control;
    control.Add(Object(CommonName), std::string("a\nb\x7f", 4));
    Expect(control.Build(), 0, SepCommaPlus | EscCtrl, "CN=a\\0Ab\\7F", "control characters become hex escapes");
    Expect(control.Build(), 0, SepCommaPlus, std::string("CN=a\nb\x7f", 7), "control characters are raw without the flag");
    Builder utf8;
    utf8.Add(Object(CommonName), "\xc3\xa9");
    Expect(utf8.Build(), 0, SepCommaPlus | EscMsb, "CN=\\E9", "a UTF8String character above 0x7f is escaped as its code point");
    Expect(utf8.Build(), 0, SepCommaPlus | EscMsb | Utf8Convert, "CN=\\C3\\A9", "UTF8 conversion treats the bytes as characters");
    Expect(utf8.Build(), 0, SepCommaPlus, std::string("CN=\xe9", 4), "UTF8String decodes to a character without conversion");
    Builder broken;
    broken.Add(Object(CommonName), "a\xc3\x28");
    ExpectFailure(broken.Build(), 0, SepCommaPlus, "CN=", "an invalid UTF8String fails after the name");
    Builder wide;
    wide.Add(Object(CommonName), std::string("\x00\xe9\x20\xac", 4), 30);
    Expect(wide.Build(), 0, SepCommaPlus | EscMsb, "CN=\\E9\\U20AC", "BMPString characters print as code points");
    Expect(wide.Build(), 0, SepCommaPlus | EscMsb | Utf8Convert, "CN=\\C3\\A9\\E2\\82\\AC", "BMPString converts to UTF-8");
    Expect(wide.Build(), 0, SepCommaPlus | Utf8Convert, "CN=\xc3\xa9\xe2\x82\xac", "converted bytes are raw without ESC_MSB");
    Builder surrogate;
    surrogate.Add(Object(CommonName), std::string("\xd8\x00", 2), 30);
    ExpectFailure(surrogate.Build(), 0, SepCommaPlus, "CN=", "a BMPString surrogate fails");
    Builder oddBmp;
    oddBmp.Add(Object(CommonName), std::string("\x00\xe9\x20", 3), 30);
    ExpectFailure(oddBmp.Build(), 0, SepCommaPlus, "CN=", "an odd-length BMPString fails");
    Builder oddUniversal;
    oddUniversal.Add(Object(CommonName), std::string("\x00\x01\xf6", 3), 28);
    ExpectFailure(oddUniversal.Build(), 0, SepCommaPlus, "CN=", "a truncated UniversalString fails");
    Builder universal;
    universal.Add(Object(CommonName), std::string("\x00\x01\xf6\x00", 4), 28);
    Expect(universal.Build(), 0, SepCommaPlus, "CN=\\W0001F600", "a UniversalString character above 0xffff prints as eight digits");
    Expect(universal.Build(), 0, SepCommaPlus | Utf8Convert, "CN=\xf0\x9f\x98\x80", "a UniversalString converts to four UTF-8 bytes");
    Builder tooBig;
    tooBig.Add(Object(CommonName), std::string("\x00\x11\x00\x00", 4), 28);
    ExpectFailure(tooBig.Build(), 0, SepCommaPlus, "CN=", "a UniversalString above U+10FFFF fails");
    Builder ia5;
    ia5.Add(Object(CommonName), "abc", 22);
    Expect(ia5.Build(), 0, SepCommaPlus | ShowType, "CN=IA5STRING:abc", "the type name is shown");
    Expect(ia5.Build(), 0, SepCommaPlus | DumpAll, "CN=#616263", "every string can be dumped");
    Expect(ia5.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#1603616263", "the DER dump includes the tag and length");
    Expect(ia5.Build(), 0, SepCommaPlus | DumpAll | ShowType, "CN=IA5STRING:#616263", "the type name precedes a dump");
    Builder octets;
    octets.Add(Object(CommonName), "ab", 4);
    Expect(octets.Build(), 0, SepCommaPlus, "CN=ab", "a non-string type is printed byte-wise without the unknown dump flag");
    Expect(octets.Build(), 0, SepCommaPlus | DumpUnknown, "CN=#6162", "a non-string type is dumped with the unknown dump flag");
    Expect(octets.Build(), 0, SepCommaPlus | DumpUnknown | IgnoreType, "CN=ab", "ignoring the type prints bytes");
    Expect(octets.Build(), 0, SepCommaPlus | DumpUnknown | DumpDer, "CN=#04026162", "an octet string is dumped with its header");
    Builder longValue;
    longValue.Add(Object(CommonName), std::string(130, 'z'), 4);
    std::string longHex;
    for (int index = 0; index < 130; ++index) longHex += "7A";
    Expect(longValue.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#048182" + longHex, "a long value uses the long length form");
    Builder tagged;
    tagged.Add(Object(CommonName), "q", 400);
    Expect(tagged.Build(), 0, SepCommaPlus | DumpAll | DumpDer | ShowType, "CN=(unknown):#1F8310" + std::string("0171"), "a high tag number uses the long tag form");
    Builder empty;
    empty.Add(Object(CommonName), "");
    Expect(empty.Build(), 0, SepCommaPlus | Esc2253, "CN=", "an empty value prints nothing");
    Expect(empty.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#0C00", "an empty value dumps its header");
}

void Der() {
    Builder integer;
    integer.Add(Object(CommonName), std::string("\x80", 1), 2);
    Expect(integer.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#02020080", "a positive integer is padded to keep its sign");
    Builder enumerated;
    enumerated.Add(Object(CommonName), std::string("\x01", 1), 10);
    Expect(enumerated.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#0A0101", "an enumerated value uses the integer content form");
    Builder bitString;
    bitString.Add(Object(CommonName), std::string("\x80", 1), 3);
    Expect(bitString.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#03020780", "a bit string dump adds the unused-bits octet");
    Builder nullValue;
    nullValue.Add(Object(CommonName), "", 5);
    Expect(nullValue.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#0500", "a null dump has no content octets");
    Builder negative;
    negative.Add(Object(CommonName), std::string("\x80", 1), 258);
    Expect(negative.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#1F82020180", "a negative ASN1_STRING type uses the high-tag form");
    Builder boolean;
    boolean.Add(Object(CommonName), "x", 1);
    const LibreSslX509Name* booleanName = boolean.Build();
    const auto* booleanEntry = reinterpret_cast<const LibreSslX509NameEntry*>(booleanName->entries->data[0]);
    char booleanByte[3];
    std::snprintf(booleanByte, sizeof(booleanByte), "%02X", static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(booleanEntry->value) & 0xffu));
    Expect(booleanName, 0, SepCommaPlus | DumpAll | DumpDer, std::string("CN=#0101") + booleanByte, "a boolean dump carries the low byte of the stored string pointer");
    Builder undef;
    undef.Add(Object(CommonName), "x", -1);
    Expect(undef.Build(), 0, SepCommaPlus | DumpAll | DumpDer, "CN=#1F0178", "an undefined type masks the tag to five bits");
}

int budget = 0;

int APS5_VABI Limited(LibreSsl::Bio*, const char*, int size) {
    if (size > budget) return -1;
    budget -= size;
    return size;
}

void WriteFailures() {
    Builder builder;
    builder.Add(Object(Country), "US").Add(Object(CommonName), "example.com");
    const LibreSslX509Name* name = builder.Build();
    LibreSsl::BioMethod method{};
    method.write = Limited;
    LibreSsl::Bio bio{};
    bio.method = &method;
    bio.initialized = 1;
    for (int allowed = 0; allowed < 20; ++allowed) {
        budget = allowed;
        const int result = X509_NAME_print_ex_nid_postfix(&bio, name, 0, SepCommaPlus);
        Require(result == (allowed >= 19 ? 19 : -1), "a write failure at any point returns -1");
    }
    for (int allowed = 0; allowed < 21; ++allowed) {
        budget = allowed;
        const int result = X509_NAME_print_ex_nid_postfix(&bio, name, 0, 0);
        Require(result == (allowed >= 20 ? 1 : 0), "a compatibility write failure returns 0");
    }
}

} // namespace

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Presets();
    Compatibility();
    Counting();
    Separators();
    Objects();
    Strings();
    Der();
    WriteFailures();
    return 0;
}
