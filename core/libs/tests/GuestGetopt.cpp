#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

extern "C" {

extern int opterr_nid_postfix;
extern int optind_nid_postfix;
extern int optopt_nid_postfix;
extern int optreset_nid_postfix;
extern char* optarg_nid_postfix;
extern FileStream* __stderrp_nid_postfix;
}

namespace {

void Expect(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what.c_str());
    std::abort();
}

void ExpectText(const std::string& actual, const std::string& expected, const std::string& what) {
    Expect(actual == expected, what + ": `" + actual + "' != `" + expected + "'");
}

class ArgumentVector {
    std::vector<std::string> storage;
    std::vector<char*> pointers;

public:
    explicit ArgumentVector(const std::string& line) {
        std::istringstream words(line);
        for (std::string word; words >> word;) storage.push_back(word);
        for (auto& word : storage) pointers.push_back(word.data());
        pointers.push_back(nullptr);
    }
    int Count() const { return static_cast<int>(storage.size()); }
    char* const* Values() { return pointers.data(); }
    std::string Joined() const {
        std::string result;
        for (std::size_t i = 0; i + 1 < pointers.size(); ++i) result += (i ? " " : "") + std::string(pointers[i]);
        return result;
    }
};

class StderrCapture {
    FileStream* previous;
    FileStream stream;

public:
    StderrCapture() : previous(__stderrp_nid_postfix), stream(TemporaryFile::Open()) { __stderrp_nid_postfix = &stream; }
    ~StderrCapture() {
        __stderrp_nid_postfix = previous;
        stream.Close();
    }
    std::string Text() {
        std::FILE* handle = stream.GetHandle();
        std::fflush(handle);
        std::rewind(handle);
        std::string text;
        for (int character; (character = std::fgetc(handle)) != EOF;) text.push_back(static_cast<char>(character));
        return text;
    }
};

void Reset() {
    optind_nid_postfix = optreset_nid_postfix = 1;
    opterr_nid_postfix = 1;
}

std::string Finish(std::string result, int count) {
    if (result.empty()) result = "|";
    else result.back() = '|';
    return result + std::to_string(count - optind_nid_postfix);
}

std::string RunGetopt(const char* options, ArgumentVector& arguments) {
    Reset();
    std::string result;
    for (int character; (character = getopt_nid_postfix(arguments.Count(), arguments.Values(), options)) != -1;) {
        Expect(character != ':', "getopt returned `:'");
        const char* found = std::strchr(options, character);
        if (!found) result += "!" + std::string(1, static_cast<char>(character)) + ",";
        else if (found[1] != ':') result += std::string(1, static_cast<char>(character)) + ",";
        else result += std::string(1, static_cast<char>(character)) + "=" + optarg_nid_postfix + ",";
    }
    return Finish(result, arguments.Count());
}

std::string RunGetoptLong(const char* options, const GuestOption* longOptions, ArgumentVector& arguments) {
    Reset();
    std::string result;
    int index = -2;
    for (int character; (character = getopt_long_nid_postfix(arguments.Count(), arguments.Values(), options, longOptions, &index)) != -1;) {
        Expect(character != ':', "getopt_long returned `:'");
        if (index == -2) {
            const char* found = std::strchr(options, character);
            if (!found) result += "!" + std::string(1, static_cast<char>(character)) + ",";
            else if (found[1] != ':') result += std::string(1, static_cast<char>(character)) + ",";
            else result += std::string(1, static_cast<char>(character)) + "=" + optarg_nid_postfix + ",";
        } else {
            const GuestOption& option = longOptions[index];
            result += "-" + std::string(option.name);
            if (option.hasArgument == 1) result += "=" + std::string(optarg_nid_postfix);
            if (option.hasArgument == 2 && optarg_nid_postfix) result += "=" + std::string(optarg_nid_postfix);
            result += ",";
        }
        index = -2;
    }
    return Finish(result, arguments.Count());
}

struct GetoptCase {
    const char* options;
    const char* arguments;
    const char* result;
};

void FreeBsdGetoptCases() {
    const GetoptCase cases[] = {
        {"c:d", "foo -c 1 -d foo", "c=1,d|1"},
        {"c:d", "foo -d foo bar", "d|2"},
        {"c:d", "foo -c 2 foo bar", "c=2|2"},
        {"c:d", "foo -e 1 foo bar", "!?|3"},
        {"c:d", "foo -d -- -c 1", "d|2"},
        {"c:d", "foo -c- 1", "c=-|1"},
        {"c:d", "foo -d - 1", "d|2"},
    };
    for (const auto& test : cases) {
        ArgumentVector arguments(test.arguments);
        StderrCapture capture;
        ExpectText(RunGetopt(test.options, arguments), test.result, std::string("getopt ") + test.arguments);
        const std::string expectedError = std::strcmp(test.result, "!?|3") == 0 ? "eboot.bin: illegal option -- e\n" : "";
        ExpectText(capture.Text(), expectedError, std::string("getopt stderr ") + test.arguments);
    }
}

void FreeBsdGetoptLongCases() {
    const GuestOption gnu[] = {
        {"required", 1, nullptr, 'r'},
        {"optional", 2, nullptr, 'o'},
        {"none", 0, nullptr, 'n'},
        {"color", 0, nullptr, 'C'},
        {"colour", 0, nullptr, 'C'},
        {nullptr, 0, nullptr, 0},
    };
    const GetoptCase cases[] = {
        {"abc:", "foo --req foobar", "-required=foobar|0"},
        {"abc:", "foo --opt=bazbug", "-optional=bazbug|0"},
        {"abc:", "foo --col", "-color|0"},
        {"abc:", "foo --colour", "-colour|0"},
        {"abc:", "foo -a -b -cfoobar --required foobar --optional=bazbug --none random --col --color --colour",
         "a,b,c=foobar,-required=foobar,-optional=bazbug,-none,-color,-color,-colour|1"},
    };
    for (const auto& test : cases) {
        ArgumentVector arguments(test.arguments);
        StderrCapture capture;
        ExpectText(RunGetoptLong(test.options, gnu, arguments), test.result, std::string("getopt_long ") + test.arguments);
        ExpectText(capture.Text(), "", std::string("getopt_long stderr ") + test.arguments);
    }

    int flags[3] {};
    const GuestOption lists[] = {
        {"list-foobar", 0, &flags[0], 'f'},
        {"list-goobar", 0, &flags[1], 'g'},
        {"list", 0, &flags[2], 'l'},
        {nullptr, 0, nullptr, 0},
    };
    ArgumentVector arguments("foo --list");
    ExpectText(RunGetoptLong("fgl", lists, arguments), "-list|0", "exact match among prefixes");
    Expect(flags[0] == 0 && flags[1] == 0 && flags[2] == 'l', "flag set by an exact match");
}

void ShortOptions() {
    ArgumentVector grouped("foo -dcvalue -dd rest");
    ExpectText(RunGetopt("c:d", grouped), "d,c=value,d,d|1", "grouped options");

    ArgumentVector optional("foo -afoo -a bar");
    Reset();
    Expect(getopt_nid_postfix(optional.Count(), optional.Values(), "a::") == 'a' && std::strcmp(optarg_nid_postfix, "foo") == 0, "optional attached");
    Expect(getopt_nid_postfix(optional.Count(), optional.Values(), "a::") == 'a' && optarg_nid_postfix == nullptr && optind_nid_postfix == 3, "optional absent");
    Expect(getopt_nid_postfix(optional.Count(), optional.Values(), "a::") == -1 && optind_nid_postfix == 3, "optional stops at operand");

    ArgumentVector dash("foo - -x");
    Reset();
    Expect(getopt_nid_postfix(dash.Count(), dash.Values(), "-x") == '-' && optind_nid_postfix == 2, "lone dash as option");
    Expect(getopt_nid_postfix(dash.Count(), dash.Values(), "-x") == 'x' && optind_nid_postfix == 3, "after lone dash");

    ArgumentVector unknown("foo -x");
    {
        StderrCapture capture;
        Reset();
        opterr_nid_postfix = 0;
        Expect(getopt_nid_postfix(unknown.Count(), unknown.Values(), "ab") == '?', "unknown option returns ?");
        Expect(optopt_nid_postfix == 'x' && optind_nid_postfix == 2, "optopt of unknown option");
        ExpectText(capture.Text(), "", "opterr 0 is silent");
    }
    {
        StderrCapture capture;
        Reset();
        Expect(getopt_nid_postfix(unknown.Count(), unknown.Values(), ":ab") == '?' && optopt_nid_postfix == 'x', "leading colon unknown");
        ExpectText(capture.Text(), "", "leading colon is silent");
    }

    ArgumentVector missing("foo -c");
    {
        StderrCapture capture;
        Reset();
        Expect(getopt_nid_postfix(missing.Count(), missing.Values(), "c:") == '?' && optopt_nid_postfix == 'c' && optind_nid_postfix == 2, "missing argument");
        ExpectText(capture.Text(), "eboot.bin: option requires an argument -- c\n", "missing argument message");
    }
    {
        StderrCapture capture;
        Reset();
        Expect(getopt_nid_postfix(missing.Count(), missing.Values(), ":c:") == ':' && optopt_nid_postfix == 'c', "missing argument with leading colon");
        ExpectText(capture.Text(), "", "leading colon missing argument is silent");
    }

    ArgumentVector first("foo -ab");
    ArgumentVector second("bar -c");
    Reset();
    Expect(getopt_nid_postfix(first.Count(), first.Values(), "abc") == 'a', "first parse");
    optind_nid_postfix = optreset_nid_postfix = 1;
    Expect(getopt_nid_postfix(second.Count(), second.Values(), "abc") == 'c', "optreset restarts getopt");
    Expect(getopt_nid_postfix(second.Count(), second.Values(), "abc") == -1 && optind_nid_postfix == 2, "restarted parse ends");

    ArgumentVector permuted("foo a -b c");
    ExpectText(RunGetopt("bc", permuted), "|3", "getopt does not permute");
}

void LongOnlyOptions() {
    int flag = 0;
    const GuestOption options[] = {
        {"alpha", 0, nullptr, 100},
        {"alpine", 0, nullptr, 100},
        {"b", 0, nullptr, 101},
        {"required", 1, nullptr, 102},
        {"optional", 2, nullptr, 103},
        {"flag", 0, &flag, 7},
        {nullptr, 0, nullptr, 0},
    };
    struct Case {
        const char* arguments;
        const char* shortOptions;
        std::vector<int> results;
        std::vector<std::string> values;
        int finalIndex;
    };
    const Case cases[] = {
        {"foo -alpha --alpha -b", "ab", {100, 100, 101}, {"", "", ""}, 4},
        {"foo -a -acvalue", "ac:", {'a', 'a', 'c'}, {"", "", "value"}, 3},
        {"foo -required=one -req two -optional=three -optional four", "", {102, 102, 103, 103}, {"one", "two", "three", ""}, 6},
        {"foo operand -alpha tail -b", "ab", {100, 101}, {"", ""}, 3},
        {"foo -al --al", "a", {'?', '?'}, {"", ""}, 3},
        {"foo -unknown", "", {'?'}, {""}, 2},
        {"foo -required", ":", {':'}, {""}, 2},
        {"foo -alpha=yes", "", {'?'}, {""}, 2},
        {"foo -flag", "", {0}, {""}, 2},
        {"foo -- -alpha", "", {}, {}, 2},
        {"foo operand -alpha", "+", {}, {}, 1},
        {"foo operand -alpha", "-", {1, 100}, {"operand", ""}, 3},
        {"foo -W alpha", "W;", {100}, {""}, 3},
    };
    for (const auto& test : cases) {
        ArgumentVector arguments(test.arguments);
        Reset();
        opterr_nid_postfix = 0;
        for (std::size_t i = 0; i < test.results.size(); ++i) {
            const int result = getopt_long_only_nid_postfix(arguments.Count(), arguments.Values(), test.shortOptions, options, nullptr);
            Expect(result == test.results[i], std::string("getopt_long_only result: ") + test.arguments);
            ExpectText(optarg_nid_postfix ? optarg_nid_postfix : "", test.values[i], "getopt_long_only argument");
        }
        Expect(getopt_long_only_nid_postfix(arguments.Count(), arguments.Values(), test.shortOptions, options, nullptr) == -1,
               std::string("getopt_long_only termination: ") + test.arguments);
        Expect(optind_nid_postfix == test.finalIndex, std::string("getopt_long_only optind: ") + test.arguments);
    }
    Expect(flag == 7, "getopt_long_only flag pointer");
    {
        ArgumentVector arguments("foo -al");
        StderrCapture capture;
        Reset();
        int index = -1;
        Expect(getopt_long_only_nid_postfix(arguments.Count(), arguments.Values(), "a", options, &index) == '?' &&
               optopt_nid_postfix == 0 && index == -1, "ambiguous aliases keep index unchanged");
        ExpectText(capture.Text(), "eboot.bin: option `-al' is ambiguous\n", "single dash diagnostic");
    }
    ArgumentVector arguments("foo -alpha");
    Reset();
    int index = -1;
    Expect(getopt_long_only_nid_postfix(arguments.Count(), arguments.Values(), "a", options, &index) == 100 && index == 0,
           "long-only index");
    optind_nid_postfix = 0;
    Expect(getopt_long_only_nid_postfix(arguments.Count(), arguments.Values(), "a", options, nullptr) == 100, "long-only restart");
}

void LongOptions() {
    int flag = 0;
    const GuestOption options[] = {
        {"required", 1, nullptr, 'r'},
        {"optional", 2, nullptr, 'o'},
        {"none", 0, nullptr, 'n'},
        {"verbose", 0, nullptr, 'v'},
        {"version", 0, nullptr, 'V'},
        {"flag", 0, &flag, 7},
        {nullptr, 0, nullptr, 0},
    };

    ArgumentVector kinds("foo --required=1 --required 2 --optional=3 --optional 4 --none --flag");
    ExpectText(RunGetoptLong("", options, kinds), "-required=1,-required=2,-optional=3,-optional,-none,-flag|1", "three argument kinds");
    Expect(flag == 7, "flag pointer set");
    ExpectText(kinds.Joined(), "foo --required=1 --required 2 --optional=3 --optional --none --flag 4", "operand permuted to the end");

    ArgumentVector abbreviations("foo --verb --vers --req=x --opt");
    ExpectText(RunGetoptLong("", options, abbreviations), "-verbose,-version,-required=x,-optional|0", "unique abbreviations");

    struct ErrorCase {
        const char* options;
        const char* arguments;
        int result;
        int optopt;
        const char* message;
    };
    const ErrorCase errors[] = {
        {"", "foo --ver", '?', 0, "eboot.bin: option `--ver' is ambiguous\n"},
        {"", "foo --none=1", '?', 'n', "eboot.bin: option `--none' doesn't allow an argument\n"},
        {"", "foo --flag=1", '?', 0, "eboot.bin: option `--flag' doesn't allow an argument\n"},
        {"", "foo --required", '?', 'r', "eboot.bin: option `--required' requires an argument\n"},
        {":", "foo --required", ':', 'r', ""},
        {"", "foo --nope=1", '?', 0, "eboot.bin: unrecognized option `--nope=1'\n"},
        {"", "foo -x", '?', 'x', "eboot.bin: invalid option -- x\n"},
        {"c:", "foo -c", '?', 'c', "eboot.bin: option requires an argument -- c\n"},
        {":c:", "foo -c", ':', 'c', ""},
        {"W;", "foo -W ver", '?', 0, "eboot.bin: option `-W ver' is ambiguous\n"},
        {"W;", "foo -W", '?', 'W', "eboot.bin: option requires an argument -- W\n"},
    };
    for (const auto& test : errors) {
        ArgumentVector arguments(test.arguments);
        StderrCapture capture;
        Reset();
        optopt_nid_postfix = -1;
        const int result = getopt_long_nid_postfix(arguments.Count(), arguments.Values(), test.options, options, nullptr);
        Expect(result == test.result, std::string("getopt_long result ") + test.arguments);
        Expect(optopt_nid_postfix == test.optopt, std::string("getopt_long optopt ") + test.arguments);
        ExpectText(capture.Text(), test.message, std::string("getopt_long message ") + test.arguments);
    }
    {
        ArgumentVector arguments("foo --ver -x");
        StderrCapture capture;
        Reset();
        opterr_nid_postfix = 0;
        Expect(getopt_long_nid_postfix(arguments.Count(), arguments.Values(), "", options, nullptr) == '?' && optopt_nid_postfix == 0, "silent ambiguity");
        Expect(getopt_long_nid_postfix(arguments.Count(), arguments.Values(), "", options, nullptr) == '?' && optopt_nid_postfix == 'x', "silent unknown");
        ExpectText(capture.Text(), "", "opterr 0 silences getopt_long");
    }

    ArgumentVector wOption("foo -W verbose -Wversion");
    ExpectText(RunGetoptLong("W;", options, wOption), "-verbose,-version|0", "W; long options");

    ArgumentVector terminated("foo a -b -- -d c");
    ExpectText(RunGetoptLong("bd", options, terminated), "b|3", "-- ends getopt_long");
    ExpectText(terminated.Joined(), "foo -b -- a -d c", "-- permutes skipped operands");

    ArgumentVector interleaved("foo a -b c -d");
    ExpectText(RunGetoptLong("bd", options, interleaved), "b,d|2", "permutation");
    ExpectText(interleaved.Joined(), "foo -b -d a c", "permuted order");

    ArgumentVector plus("foo a -b");
    ExpectText(RunGetoptLong("+b", options, plus), "|2", "+ stops at the first operand");

    ArgumentVector inOrder("foo a -b c");
    Reset();
    std::string sequence;
    for (int character; (character = getopt_long_nid_postfix(inOrder.Count(), inOrder.Values(), "-b", options, nullptr)) != -1;)
        sequence += character == 1 ? std::string("1:") + optarg_nid_postfix + "," : std::string(1, static_cast<char>(character)) + ",";
    ExpectText(sequence, "1:a,b,1:c,", "- returns operands in order");

    ArgumentVector restartFirst("foo -bd");
    ArgumentVector restartSecond("bar -b");
    Reset();
    Expect(getopt_long_nid_postfix(restartFirst.Count(), restartFirst.Values(), "bd", options, nullptr) == 'b', "getopt_long first parse");
    optind_nid_postfix = 0;
    Expect(getopt_long_nid_postfix(restartSecond.Count(), restartSecond.Values(), "bd", options, nullptr) == 'b', "optind 0 restarts getopt_long");
    Expect(getopt_long_nid_postfix(restartSecond.Count(), restartSecond.Values(), "bd", options, nullptr) == -1 && optind_nid_postfix == 2, "restarted getopt_long ends");
    Reset();
    Expect(getopt_long_nid_postfix(restartFirst.Count(), restartFirst.Values(), "bd", options, nullptr) == 'b', "getopt_long parse before optreset");
    optind_nid_postfix = optreset_nid_postfix = 1;
    Expect(getopt_long_nid_postfix(restartSecond.Count(), restartSecond.Values(), "bd", options, nullptr) == 'b', "optreset restarts getopt_long");

    Expect(setenv_nid_postfix("POSIXLY_CORRECT", "1", 1) == 0, "setenv POSIXLY_CORRECT");
    {
        ArgumentVector posix("foo a -b");
        ExpectText(RunGetoptLong("b", options, posix), "|2", "POSIXLY_CORRECT stops at the first operand");
        ArgumentVector illegal("foo -x");
        StderrCapture capture;
        Reset();
        Expect(getopt_long_nid_postfix(illegal.Count(), illegal.Values(), "b", options, nullptr) == '?', "POSIXLY_CORRECT unknown option");
        ExpectText(capture.Text(), "eboot.bin: illegal option -- x\n", "POSIXLY_CORRECT message");
    }
    Expect(unsetenv_nid_postfix("POSIXLY_CORRECT") == 0, "unsetenv POSIXLY_CORRECT");
    ArgumentVector gnu("foo a -b");
    ExpectText(RunGetoptLong("b", options, gnu), "b|1", "permutation after POSIXLY_CORRECT is unset");

    Reset();
    Expect(getopt_long_nid_postfix(gnu.Count(), gnu.Values(), nullptr, options, nullptr) == -1, "null option string");
}

}

int main() {
    Expect(opterr_nid_postfix == 1 && optind_nid_postfix == 1 && optopt_nid_postfix == 0 &&
           optreset_nid_postfix == 0 && optarg_nid_postfix == nullptr, "initial values");
    FreeBsdGetoptCases();
    FreeBsdGetoptLongCases();
    ShortOptions();
    LongOptions();
    LongOnlyOptions();
}
