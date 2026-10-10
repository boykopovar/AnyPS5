#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#ifndef _WIN32
#include <unistd.h>
#endif

extern "C" {
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, FileStream*);
std::size_t APS5_VABI fwrite_nid_postfix(const void*, std::size_t, std::size_t, FileStream*);
FileStream* APS5_VABI freopen_nid_postfix(const char*, const char*, FileStream*);
int APS5_VABI fseeko_nid_postfix(FileStream*, std::int64_t, int);
std::int64_t APS5_VABI ftello_nid_postfix(FileStream*);
int APS5_VABI fseek_nid_postfix(FileStream*, std::int64_t, int);
std::int64_t APS5_VABI ftell_nid_postfix(FileStream*);
int* APS5_VABI __error_nid_postfix();
extern FileStream* __stdinp_nid_postfix;
extern FileStream* __stdoutp_nid_postfix;
extern FileStream* __stderrp_nid_postfix;
extern int __isthreaded_nid_postfix;
int APS5_VABI fprintf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI vfprintf_nid_postfix(FileStream*, const char*, void*);
int APS5_VABI vsprintf_nid_postfix(char*, const char*, void*);
int APS5_VABI fgetc_nid_postfix(FileStream*);
int APS5_VABI fputc_nid_postfix(int, FileStream*);
int APS5_VABI fputwc_nid_postfix(char16_t, FileStream*);
int APS5_VABI fputws_nid_postfix(const char16_t*, FileStream*);
int APS5_VABI fscanf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI __srget_nid_postfix(FileStream*);
int APS5_VABI __swbuf_nid_postfix(int, FileStream*);
int APS5_VABI ungetc_nid_postfix(int, FileStream*);
char* APS5_VABI fgets_nid_postfix(char*, int, FileStream*);
int APS5_VABI feof_nid_postfix(FileStream*);
int APS5_VABI fileno_nid_postfix(FileStream*);
void APS5_VABI clearerr_nid_postfix(FileStream*);
int APS5_VABI setvbuf_nid_postfix(FileStream*, char*, int, std::size_t);
void APS5_VABI setbuf_nid_postfix(FileStream*, char*);
FileStream* APS5_VABI fdopen_nid_postfix(int, const char*);
int APS5_VABI fclose_nid_postfix(FileStream*);
int APS5_VABI _Getmbcurmax_nid_postfix();
int APS5_VABI ___mb_cur_max_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;
constexpr int ebadf = 9;
constexpr int einval = 22;
constexpr int enotsup = 45;
constexpr std::int16_t eofFlag = 0x20;
constexpr std::int64_t largeOffset = INT64_C(4294967313);

int APS5_VABI WriteFormatted(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_nid_postfix(stream, format, args);
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI FormatString(char* buffer, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vsprintf_nid_postfix(buffer, format, args);
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int Duplicate(int descriptor) {
#ifdef _WIN32
    return _dup(descriptor);
#else
    return ::dup(descriptor);
#endif
}

int CloseDescriptor(int descriptor) {
#ifdef _WIN32
    return _close(descriptor);
#else
    return ::close(descriptor);
#endif
}

int HostDescriptor(std::FILE* file) {
#ifdef _WIN32
    return _fileno(file);
#else
    return ::fileno(file);
#endif
}

std::FILE* OpenTemporaryFile() {
    std::FILE* file = std::tmpfile();
    Require(file != nullptr, "create a temporary file");
    return file;
}

class TemporaryStream {
public:
    TemporaryStream() : stream(OpenTemporaryFile()) {}

    ~TemporaryStream() {
        try {
            stream.Close();
        } catch (const std::exception&) {
        }
    }

    TemporaryStream(const TemporaryStream&) = delete;
    TemporaryStream& operator=(const TemporaryStream&) = delete;

    FileStream stream;
};

class OpenedStream {
public:
    explicit OpenedStream(FileStream* stream) : stream(stream) {}

    ~OpenedStream() {
        if (stream == nullptr) return;
        try {
            fclose_nid_postfix(stream);
        } catch (const std::exception&) {
        }
    }

    OpenedStream(const OpenedStream&) = delete;
    OpenedStream& operator=(const OpenedStream&) = delete;

    FileStream* Get() const { return stream; }

    int Close() {
        FileStream* closing = stream;
        stream = nullptr;
        return fclose_nid_postfix(closing);
    }

private:
    FileStream* stream;
};

class ScratchDirectory {
public:
    explicit ScratchDirectory(const char* prefix)
        : name(prefix + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          path(std::filesystem::current_path() / name) {
        Require(std::filesystem::create_directory(path), "create the scratch directory");
    }

    ~ScratchDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    std::string GuestPath(const char* file) const { return name + "/" + file; }

    std::string Contents(const char* file) const {
        std::ifstream input(path / file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    const std::string name;
    const std::filesystem::path path;
};

class DescriptorFixture {
public:
    DescriptorFixture() : original(OpenTemporaryFile()) {
        std::fputs("retained", original);
        std::fflush(original);
        descriptor = Duplicate(HostDescriptor(original));
        if (descriptor < 0) {
            std::fclose(original);
            Testing::Fail("duplicate the temporary file descriptor");
        }
    }

    ~DescriptorFixture() {
        if (wrapped != nullptr) {
            try {
                fclose_nid_postfix(wrapped);
            } catch (const std::exception&) {
            }
        } else if (!descriptorTransferred) {
            CloseDescriptor(descriptor);
        }
        if (original != nullptr) std::fclose(original);
    }

    DescriptorFixture(const DescriptorFixture&) = delete;
    DescriptorFixture& operator=(const DescriptorFixture&) = delete;

    FileStream* Wrap() {
        wrapped = fdopen_nid_postfix(descriptor, "r+b");
        Require(wrapped != nullptr, "fdopen the duplicated descriptor");
        descriptorTransferred = true;
        return wrapped;
    }

    int CloseWrapped() {
        FileStream* closing = wrapped;
        wrapped = nullptr;
        return fclose_nid_postfix(closing);
    }

    int CloseOriginal() {
        std::FILE* closing = original;
        original = nullptr;
        return std::fclose(closing);
    }

    std::FILE* original;
    int descriptor = -1;

private:
    FileStream* wrapped = nullptr;
    bool descriptorTransferred = false;
};

void RequireEofSet(FileStream& stream, const char* message) {
    Require(feof_nid_postfix(&stream) != 0, std::string(message) + ": feof");
    Require((stream.GuestState().flags & eofFlag) != 0, std::string(message) + ": guest EOF flag");
}

const Case mbCurMax{"MbCurMax_BothEntryPoints_ReturnOne", [] {
    RequireEqual(_Getmbcurmax_nid_postfix(), 1, "_Getmbcurmax");
    RequireEqual(___mb_cur_max_nid_postfix(), _Getmbcurmax_nid_postfix(), "___mb_cur_max");
}};

const Case fdopenNegative{"Fdopen_NegativeDescriptor_FailsWithEbadf", [] {
    Require(fdopen_nid_postfix(-1, "rb") == nullptr, "fdopen result");
    RequireEqual(*__error_nid_postfix(), ebadf, "errno");
}};

const Case fdopenNullMode{"Fdopen_NullMode_FailsWithEinval", [] {
    Require(fdopen_nid_postfix(0, nullptr) == nullptr, "fdopen result");
    RequireEqual(*__error_nid_postfix(), einval, "errno");
}};

const Case fdopenInvalidMode{"Fdopen_UnknownMode_FailsWithEinval", [] {
    Require(fdopen_nid_postfix(0, "invalid") == nullptr, "fdopen result");
    RequireEqual(*__error_nid_postfix(), einval, "errno");
}};

const Case fdopenReads{"Fdopen_DuplicatedDescriptor_ReadsExistingContents", [] {
    DescriptorFixture fixture;
    FileStream* wrapped = fixture.Wrap();
    RequireEqual(fileno_nid_postfix(wrapped), fixture.descriptor, "fileno");
    setbuf_nid_postfix(wrapped, nullptr);
    RequireEqual(fseek_nid_postfix(wrapped, 0, SEEK_SET), 0, "fseek");
    char contents[32]{};
    Require(fgets_nid_postfix(contents, sizeof(contents), wrapped) == contents, "fgets returns the buffer");
    RequireEqual(std::string_view(contents), std::string_view("retained"), "contents");
}};

const Case fdopenClose{"Fclose_FdopenedStream_ClosesOnlyItsDescriptor", [] {
    DescriptorFixture fixture;
    fixture.Wrap();
    RequireEqual(fixture.CloseWrapped(), 0, "fclose");
    RequireEqual(CloseDescriptor(fixture.descriptor), -1, "descriptor already closed");
    RequireEqual(fixture.CloseOriginal(), 0, "original stream still closes");
}};

const Case vsprintfMixed{"Vsprintf_MixedRegisterAndStackArguments_FormatsAndStoresCount", [] {
    char output[256];
    std::memset(output, '!', sizeof(output));
    std::int64_t count = -1;
    const std::string_view expected = "guest:4294967297:  3.50:1,2,3,4,5,6,7,8:%";
    const int written = FormatString(output, "%s:%ld:%*.*f:%d,%d,%d,%d,%d,%d,%d,%d:%%%ln",
        "guest", std::int64_t{4294967297}, 6, 2, 3.5, 1, 2, 3, 4, 5, 6, 7, 8, &count);
    RequireEqual(written, static_cast<int>(expected.size()), "return value");
    RequireEqual(count, static_cast<std::int64_t>(written), "%ln count");
    RequireEqual(std::string_view(output), expected, "output");
    RequireEqual(output[written + 1], '!', "byte after the terminator untouched");
}};

const Case vsprintfFloats{"Vsprintf_ManyFloatingArguments_FormatsRegisterAndStackValues", [] {
    char output[256];
    RequireEqual(FormatString(output, "%.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.2Lf",
        1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 1.25L), 25, "return value");
    RequireEqual(std::string_view(output), std::string_view("1 2 3 4 5 6 7 8 9 10 1.25"), "output");
}};

const Case vsprintfEmpty{"Vsprintf_EmptyFormat_WritesOnlyTerminator", [] {
    char output[8];
    std::memset(output, '!', sizeof(output));
    RequireEqual(FormatString(output, ""), 0, "return value");
    RequireEqual(output[0], '\0', "terminator");
}};

const Case isThreaded{"IsThreaded_Global_IsOne", [] {
    RequireEqual(__isthreaded_nid_postfix, 1, "__isthreaded");
}};

const Case streamPointers{"StandardStreamPointers_StdoutAndStderr_ReferToGuestStreams", [] {
    Require(__stdoutp_nid_postfix == &_Stdout_nid_postfix, "__stdoutp");
    Require(__stderrp_nid_postfix == &_Stderr_nid_postfix, "__stderrp");
}};

const Case streamDescriptors{"StandardStreamPointers_Fileno_ReturnsZeroOneTwo", [] {
    RequireEqual(fileno_nid_postfix(__stdinp_nid_postfix), 0, "stdin");
    RequireEqual(fileno_nid_postfix(__stdoutp_nid_postfix), 1, "stdout");
    RequireEqual(fileno_nid_postfix(__stderrp_nid_postfix), 2, "stderr");
}};

const Case guestPrefix{"GuestFilePrefix_NewStream_IsGuestStateWithEmptyBuffer", [] {
    TemporaryStream temporary;
    auto& guest = *reinterpret_cast<GuestFilePrefix*>(&temporary.stream);
    Require(&guest == &temporary.stream.GuestState(), "prefix is the guest state");
    Require(guest.position == nullptr, "position");
    RequireEqual(guest.readRemaining, 0, "read remaining");
    RequireEqual(guest.writeRemaining, 0, "write remaining");
}};

const Case guestDescriptor{"GuestFilePrefix_NewStream_StoresHostDescriptor", [] {
    TemporaryStream temporary;
    RequireEqual(static_cast<int>(temporary.stream.GuestState().descriptor), fileno_nid_postfix(&temporary.stream), "descriptor");
}};

const Case setvbufModes{"Setvbuf_EachBufferingMode_Succeeds", [] {
    for (const int mode : {1, 0, 2}) {
        TemporaryStream temporary;
        RequireEqual(setvbuf_nid_postfix(&temporary.stream, nullptr, mode, 0), 0, "mode " + std::to_string(mode));
    }
}};

void WriteThroughSwbuf(FileStream& stream) {
    RequireEqual(setvbuf_nid_postfix(&stream, nullptr, 2, 0), 0, "setvbuf unbuffered");
    RequireEqual(fputc_nid_postfix('A', &stream), static_cast<int>('A'), "fputc");
    Require(--stream.GuestState().writeRemaining < 0, "write count exhausted");
    RequireEqual(__swbuf_nid_postfix('\n', &stream), static_cast<int>('\n'), "__swbuf");
}

void ReadThroughSrget(FileStream& stream) {
    WriteThroughSwbuf(stream);
    std::rewind(stream.GetHandle());
    Require(--stream.GuestState().readRemaining < 0, "read count exhausted");
    RequireEqual(__srget_nid_postfix(&stream), static_cast<int>('A'), "__srget");
}

void ReadPushedBackLine(FileStream& stream) {
    ReadThroughSrget(stream);
    RequireEqual(ungetc_nid_postfix('B', &stream), static_cast<int>('B'), "ungetc");
    char text[8]{};
    Require(fgets_nid_postfix(text, sizeof(text), &stream) == text, "fgets returns the buffer");
    RequireEqual(std::string_view(text), std::string_view("B\n"), "line");
}

void ReadToEnd(FileStream& stream) {
    ReadPushedBackLine(stream);
    RequireEqual(fgetc_nid_postfix(&stream), EOF, "fgetc at the end");
    RequireEofSet(stream, "after reading past the end");
}

const Case swbufCase{"Swbuf_WriteCountExhausted_WritesCharacter", [] {
    TemporaryStream temporary;
    WriteThroughSwbuf(temporary.stream);
}};

const Case srgetCase{"Srget_ReadCountExhausted_ReadsCharacter", [] {
    TemporaryStream temporary;
    ReadThroughSrget(temporary.stream);
}};

const Case ungetcCase{"Ungetc_AfterSrget_IsReadFirstByFgets", [] {
    TemporaryStream temporary;
    ReadPushedBackLine(temporary.stream);
}};

const Case fgetcEnd{"Fgetc_AtEndOfStream_ReturnsEofAndSetsEofFlag", [] {
    TemporaryStream temporary;
    ReadToEnd(temporary.stream);
}};

const Case clearerrCase{"Clearerr_AfterEndOfStream_ClearsEofFlag", [] {
    TemporaryStream temporary;
    ReadToEnd(temporary.stream);
    clearerr_nid_postfix(&temporary.stream);
    RequireEqual(feof_nid_postfix(&temporary.stream), 0, "feof");
    RequireEqual(temporary.stream.GuestState().flags & eofFlag, 0, "guest EOF flag");
}};

const Case closeStream{"FileStreamClose_UsedStream_ClearsFlagsAndDescriptor", [] {
    TemporaryStream temporary;
    ReadToEnd(temporary.stream);
    clearerr_nid_postfix(&temporary.stream);
    temporary.stream.Close();
    RequireEqual(static_cast<int>(temporary.stream.GuestState().flags), 0, "flags");
    RequireEqual(static_cast<int>(temporary.stream.GuestState().descriptor), -1, "descriptor");
}};

const Case wideOutput{"Fputwc_Fputws_WriteUtf8Bytes", [] {
    TemporaryStream temporary;
    RequireEqual(fputwc_nid_postfix(u'A', &temporary.stream), static_cast<int>(u'A'), "fputwc");
    RequireEqual(fputws_nid_postfix(u"B\x00E9", &temporary.stream), 2, "fputws");
    std::rewind(temporary.stream.GetHandle());
    char bytes[8]{};
    RequireEqual(std::fread(bytes, 1, 4, temporary.stream.GetHandle()), std::size_t{4}, "byte count");
    RequireEqual(std::string_view(bytes, 4), std::string_view("AB\xC3\xA9", 4), "bytes");
}};

const char formattedLine[] = "guest 4294967297 1.25 1 2 3 4 5 6 7 8\n";

void WriteFormattedLine(FileStream& stream) {
    RequireEqual(fprintf_nid_postfix(&stream, "%s %ld %.2f %d %d %d %d %d %d %d %d\n",
        "guest", std::int64_t{4294967297}, 1.25, 1, 2, 3, 4, 5, 6, 7, 8), static_cast<int>(sizeof(formattedLine) - 1), "fprintf");
}

const Case fprintfLine{"Fprintf_MixedRegisterAndStackArguments_WritesFormattedLine", [] {
    TemporaryStream temporary;
    WriteFormattedLine(temporary.stream);
    std::rewind(temporary.stream.GetHandle());
    char output[128]{};
    Require(fgets_nid_postfix(output, sizeof(output), &temporary.stream) == output, "fgets returns the buffer");
    RequireEqual(std::string_view(output), std::string_view(formattedLine), "line");
}};

const Case vfprintfText{"Vfprintf_StarWidthAndPrecision_AppendsFormattedText", [] {
    TemporaryStream temporary;
    WriteFormattedLine(temporary.stream);
    RequireEqual(WriteFormatted(&temporary.stream, "%*.*f:%s", 6, 2, 3.5, "end"), 10, "vfprintf");
    std::rewind(temporary.stream.GetHandle());
    char output[128]{};
    Require(fgets_nid_postfix(output, sizeof(output), &temporary.stream) == output, "fgets first line");
    Require(fgets_nid_postfix(output, sizeof(output), &temporary.stream) == output, "fgets second line");
    RequireEqual(std::string_view(output), std::string_view("  3.50:end"), "second line");
}};

void WriteAnswer(FileStream& stream) {
    RequireEqual(fprintf_nid_postfix(&stream, "%d %s", 42, "answer"), 9, "fprintf");
    std::rewind(stream.GetHandle());
}

void ScanAnswer(FileStream& stream, int& number, char (&word)[16]) {
    WriteAnswer(stream);
    RequireEqual(fscanf_nid_postfix(&stream, "%*d"), 0, "suppressed fscanf");
    std::rewind(stream.GetHandle());
    RequireEqual(fscanf_nid_postfix(&stream, "%d %15s", &number, word), 2, "fscanf assignments");
}

const Case scanSuppressed{"Fscanf_SuppressedConversion_ReturnsZeroAndConsumesNumber", [] {
    TemporaryStream temporary;
    WriteAnswer(temporary.stream);
    RequireEqual(fscanf_nid_postfix(&temporary.stream, "%*d"), 0, "fscanf");
    RequireEqual(ftello_nid_postfix(&temporary.stream), std::int64_t{2}, "position");
}};

const Case scanAssigns{"Fscanf_NumberAndWord_AssignsRegisterArguments", [] {
    TemporaryStream temporary;
    int number = 0;
    char word[16]{};
    ScanAnswer(temporary.stream, number, word);
    RequireEqual(number, 42, "number");
    RequireEqual(std::string_view(word), std::string_view("answer"), "word");
}};

const Case scanEnd{"Fscanf_AtEndOfStream_ReturnsEofAndSetsEofFlag", [] {
    TemporaryStream temporary;
    int number = 0;
    char word[16]{};
    ScanAnswer(temporary.stream, number, word);
    RequireEqual(fscanf_nid_postfix(&temporary.stream, "%d", &number), EOF, "fscanf");
    RequireEofSet(temporary.stream, "after fscanf at the end");
}};

struct ManyValues {
    int numbers[8]{};
    std::int64_t large = 0;
    std::int64_t negative = 0;
    std::uint64_t sized = 0;
    char letters[4]{};
    std::int64_t consumed = -1;
};

ManyValues ScanMany(FileStream& stream) {
    Require(std::fputs("7 1 2 3 4 5 6 7 8 4294967297 -4294967298 4294967299 abc %!", stream.GetHandle()) >= 0, "write the input");
    std::rewind(stream.GetHandle());
    ManyValues values;
    RequireEqual(fscanf_nid_postfix(&stream, "%*d %d %d %d %d %d %d %d %d %ld %jd %zu %3[a-z] %%%ln",
        &values.numbers[0], &values.numbers[1], &values.numbers[2], &values.numbers[3], &values.numbers[4],
        &values.numbers[5], &values.numbers[6], &values.numbers[7], &values.large, &values.negative, &values.sized,
        values.letters, &values.consumed), 12, "fscanf assignments");
    return values;
}

const Case scanMany{"Fscanf_ManyConversions_AssignsRegisterAndStackArguments", [] {
    TemporaryStream temporary;
    const ManyValues values = ScanMany(temporary.stream);
    for (int index = 0; index < 8; ++index) RequireEqual(values.numbers[index], index + 1, "number " + std::to_string(index));
    RequireEqual(values.large, INT64_C(4294967297), "%ld");
    RequireEqual(values.negative, -INT64_C(4294967298), "%jd");
    RequireEqual(values.sized, UINT64_C(4294967299), "%zu");
    RequireEqual(std::string_view(values.letters), std::string_view("abc"), "%3[a-z]");
    RequireEqual(values.consumed, ftello_nid_postfix(&temporary.stream), "%ln");
}};

const Case scanLiteral{"Fscanf_LiteralPercent_StopsAfterMatchedInput", [] {
    TemporaryStream temporary;
    ScanMany(temporary.stream);
    RequireEqual(fgetc_nid_postfix(&temporary.stream), static_cast<int>('!'), "next character");
}};

const Case scanMismatch{"Fscanf_MatchingFailure_ReturnsZeroAndKeepsArgumentAndInput", [] {
    TemporaryStream temporary;
    ScanMany(temporary.stream);
    RequireEqual(fgetc_nid_postfix(&temporary.stream), static_cast<int>('!'), "next character");
    int unmatched = 123;
    RequireEqual(fseeko_nid_postfix(&temporary.stream, -1, SEEK_CUR), 0, "step back");
    RequireEqual(fscanf_nid_postfix(&temporary.stream, "%d", &unmatched), 0, "fscanf");
    RequireEqual(unmatched, 123, "argument");
    RequireEqual(fgetc_nid_postfix(&temporary.stream), static_cast<int>('!'), "unconsumed character");
}};

const Case scanAfterEnd{"Fscanf_AfterLastCharacter_ReturnsEofAndKeepsArgument", [] {
    TemporaryStream temporary;
    ScanMany(temporary.stream);
    RequireEqual(fgetc_nid_postfix(&temporary.stream), static_cast<int>('!'), "last character");
    int unmatched = 123;
    RequireEqual(fscanf_nid_postfix(&temporary.stream, "%d", &unmatched), EOF, "fscanf");
    RequireEqual(unmatched, 123, "argument");
}};

void SeekLarge(FileStream& stream) {
    RequireEqual(fseeko_nid_postfix(&stream, largeOffset, SEEK_SET), 0, "fseeko to the large offset");
}

const Case seekLarge{"Fseeko_LargeOffset_ReportedByFtelloAndFtell", [] {
    TemporaryStream temporary;
    SeekLarge(temporary.stream);
    RequireEqual(ftello_nid_postfix(&temporary.stream), largeOffset, "ftello");
    RequireEqual(ftell_nid_postfix(&temporary.stream), largeOffset, "ftell");
}};

const Case seekBack{"Fseek_NegativeRelativeOffset_MovesBackward", [] {
    TemporaryStream temporary;
    SeekLarge(temporary.stream);
    RequireEqual(fseek_nid_postfix(&temporary.stream, -9, SEEK_CUR), 0, "fseek");
    RequireEqual(ftello_nid_postfix(&temporary.stream), largeOffset - 9, "ftello");
}};

const Case seekAbsolute{"Fseek_LargeAbsoluteOffset_ReportedByFtell", [] {
    TemporaryStream temporary;
    SeekLarge(temporary.stream);
    RequireEqual(fseek_nid_postfix(&temporary.stream, -9, SEEK_CUR), 0, "fseek back");
    RequireEqual(fseek_nid_postfix(&temporary.stream, largeOffset, SEEK_SET), 0, "fseek");
    RequireEqual(ftell_nid_postfix(&temporary.stream), largeOffset, "ftell");
}};

const Case seekInvalid{"Fseeko_UnknownWhence_FailsWithEinvalAndKeepsPosition", [] {
    TemporaryStream temporary;
    SeekLarge(temporary.stream);
    RequireEqual(fseeko_nid_postfix(&temporary.stream, 0, 12345), -1, "fseeko");
    RequireEqual(*__error_nid_postfix(), einval, "errno");
    RequireEqual(ftello_nid_postfix(&temporary.stream), largeOffset, "position");
}};

void SeekEndAfterLarge(FileStream& stream) {
    SeekLarge(stream);
    RequireEqual(fseeko_nid_postfix(&stream, 0, SEEK_END), 0, "fseeko to the end");
}

const Case seekEnd{"Fseeko_EndAfterSeekingPastEnd_ReportsUnextendedSize", [] {
    TemporaryStream temporary;
    SeekEndAfterLarge(temporary.stream);
    RequireEqual(ftello_nid_postfix(&temporary.stream), std::int64_t{0}, "ftello");
}};

const Case seekEndRead{"Fgetc_AtEndOfEmptyFile_ReturnsEofAndSetsEof", [] {
    TemporaryStream temporary;
    SeekEndAfterLarge(temporary.stream);
    RequireEqual(fgetc_nid_postfix(&temporary.stream), EOF, "fgetc");
    Require(feof_nid_postfix(&temporary.stream) != 0, "feof");
}};

const Case seekClearsEof{"Fseeko_AfterEndOfFile_ClearsEof", [] {
    TemporaryStream temporary;
    SeekEndAfterLarge(temporary.stream);
    RequireEqual(fgetc_nid_postfix(&temporary.stream), EOF, "fgetc");
    RequireEqual(fseeko_nid_postfix(&temporary.stream, 0, SEEK_SET), 0, "fseeko");
    RequireEqual(feof_nid_postfix(&temporary.stream), 0, "feof");
}};

void ReopenForUpdate(FileStream& stream, const std::string& path) {
    RequireEqual(fgetc_nid_postfix(&stream), EOF, "fgetc on the empty stream");
    Require(feof_nid_postfix(&stream) != 0, "feof before freopen");
    Require(freopen_nid_postfix(path.c_str(), "w+b", &stream) == &stream, "freopen w+b returns the stream");
}

void WriteAndAppend(FileStream& stream, const std::string& path) {
    ReopenForUpdate(stream, path);
    RequireEqual(fputc_nid_postfix('R', &stream), static_cast<int>('R'), "fputc R");
    Require(freopen_nid_postfix(path.c_str(), "ab", &stream) == &stream, "freopen ab returns the stream");
    RequireEqual(fputc_nid_postfix('S', &stream), static_cast<int>('S'), "fputc S");
}

const Case reopenClearsEof{"Freopen_StreamAtEof_ReturnsSameStreamWithEofCleared", [] {
    const ScratchDirectory scratch("anyps5-reopen-test-");
    TemporaryStream temporary;
    ReopenForUpdate(temporary.stream, scratch.GuestPath("file"));
    RequireEqual(feof_nid_postfix(&temporary.stream), 0, "feof");
}};

const Case reopenUpdate{"Freopen_UpdateMode_ReadsBackWrittenByte", [] {
    const ScratchDirectory scratch("anyps5-reopen-test-");
    TemporaryStream temporary;
    ReopenForUpdate(temporary.stream, scratch.GuestPath("file"));
    RequireEqual(fputc_nid_postfix('R', &temporary.stream), static_cast<int>('R'), "fputc");
    RequireEqual(fseeko_nid_postfix(&temporary.stream, 0, SEEK_SET), 0, "fseeko");
    RequireEqual(fgetc_nid_postfix(&temporary.stream), static_cast<int>('R'), "fgetc");
}};

const Case reopenAppend{"Freopen_AppendMode_AppendsAfterExistingContents", [] {
    const ScratchDirectory scratch("anyps5-reopen-test-");
    TemporaryStream temporary;
    WriteAndAppend(temporary.stream, scratch.GuestPath("file"));
    temporary.stream.Close();
    RequireEqual(scratch.Contents("file"), std::string("RS"), "file contents");
}};

const Case reopenNullPath{"Freopen_NullPath_FailsWithEnotsupAndKeepsWrittenData", [] {
    const ScratchDirectory scratch("anyps5-reopen-test-");
    TemporaryStream temporary;
    WriteAndAppend(temporary.stream, scratch.GuestPath("file"));
    Require(freopen_nid_postfix(nullptr, "r", &temporary.stream) == nullptr, "freopen result");
    RequireEqual(*__error_nid_postfix(), enotsup, "errno");
    temporary.stream.Close();
    RequireEqual(scratch.Contents("file"), std::string("RS"), "file contents");
}};

const Case reopenMissing{"Freopen_MissingFile_FailsWithEnoentAndClosesStream", [] {
    const ScratchDirectory scratch("anyps5-reopen-test-");
    TemporaryStream temporary;
    Require(freopen_nid_postfix(scratch.GuestPath("file").c_str(), "rb", &temporary.stream) == nullptr, "freopen result");
    RequireEqual(*__error_nid_postfix(), enoent, "errno");
    RequireEqual(static_cast<int>(temporary.stream.GuestState().flags), 0, "flags");
    RequireEqual(static_cast<int>(temporary.stream.GuestState().descriptor), -1, "descriptor");
}};

const std::string originalBytes("A\r\n\x1a" "B\0C", 7);
const std::string writtenBytes("D\n\x1a" "E\0F", 6);
const char* const modes[] = {"r", "r+", "w", "w+", "a", "a+", "rb", "rb+", "r+b", "wb", "wb+", "w+b", "ab", "ab+", "a+b"};

void SeedFile(const std::filesystem::path& path) {
    std::ofstream seed(path, std::ios::binary | std::ios::trunc);
    seed.write(originalBytes.data(), static_cast<std::streamsize>(originalBytes.size()));
    Require(seed.good(), "seed the file");
}

std::string TransferBytes(FileStream* stream, const char* mode, const std::string& label) {
    const bool update = std::strchr(mode, '+') != nullptr;
    if (*mode == 'r' || (*mode == 'a' && update)) {
        RequireEqual(fseeko_nid_postfix(stream, 0, SEEK_SET), 0, label + ": seek to the start");
        char bytes[16]{};
        const std::size_t count = fread_nid_postfix(bytes, 1, sizeof(bytes), stream);
        RequireEqual(std::string(bytes, count), originalBytes, label + ": input bytes");
        RequireEqual(fseeko_nid_postfix(stream, 3, SEEK_SET), 0, label + ": seek to byte 3");
        RequireEqual(fgetc_nid_postfix(stream), 0x1a, label + ": byte 3");
    }
    if (*mode == 'w' || *mode == 'a' || update) {
        RequireEqual(fseeko_nid_postfix(stream, 0, SEEK_SET), 0, label + ": seek before writing");
        RequireEqual(fwrite_nid_postfix(writtenBytes.data(), 1, writtenBytes.size(), stream), writtenBytes.size(), label + ": fwrite");
        if (*mode == 'w') return writtenBytes;
        if (*mode == 'a') return originalBytes + writtenBytes;
        return writtenBytes + originalBytes.substr(writtenBytes.size());
    }
    return originalBytes;
}

const Case fopenModes{"Fopen_EveryMode_TransfersRawBytes", [] {
    const ScratchDirectory scratch("anyps5-byte-stream-");
    for (const char* mode : modes) {
        const std::string label = std::string("fopen ") + mode;
        SeedFile(scratch.path / "bytes");
        OpenedStream stream(fopen_nid_postfix(scratch.GuestPath("bytes").c_str(), mode));
        Require(stream.Get() != nullptr, label + ": open");
        const std::string expected = TransferBytes(stream.Get(), mode, label);
        RequireEqual(stream.Close(), 0, label + ": fclose");
        RequireEqual(scratch.Contents("bytes"), expected, label + ": file bytes");
    }
}};

const Case freopenModes{"Freopen_EveryMode_TransfersRawBytes", [] {
    const ScratchDirectory scratch("anyps5-byte-stream-");
    for (const char* mode : modes) {
        const std::string label = std::string("freopen ") + mode;
        SeedFile(scratch.path / "bytes");
        TemporaryStream redirected;
        FileStream* stream = freopen_nid_postfix(scratch.GuestPath("bytes").c_str(), mode, &redirected.stream);
        Require(stream != nullptr, label + ": open");
        const std::string expected = TransferBytes(stream, mode, label);
        RequireEqual(fclose_nid_postfix(stream), 0, label + ": fclose");
        RequireEqual(scratch.Contents("bytes"), expected, label + ": file bytes");
    }
}};

} // namespace
