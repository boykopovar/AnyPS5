#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, std::source_location where = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 error report assertion failed at line %u\n", where.line());
    std::exit(1);
}

struct Report {
    std::vector<std::string> lines;
    int result = 1;
    bool enqueueOnFirst = false;
};

int APS5_VABI Capture_nid_no_patch(const char* text, std::size_t length, void* userData) {
    auto& report = *static_cast<Report*>(userData);
    Require(length > 0 && length < 4096 && text[length] == '\0');
    report.lines.emplace_back(text, length);
    if (report.enqueueOnFirst && report.lines.size() == 1)
        LibreSslPutError_nid_no_patch(20, 102, 67, "nested.c", 14);
    return report.result;
}

std::string ThreadPrefix() {
    return std::to_string(reinterpret_cast<std::uintptr_t>(pthread_self_nid_postfix())) + ':';
}

void CheckFifo(const std::string& prefix) {
    Report report;
    ERR_print_errors_cb_nid_postfix(Capture_nid_no_patch, &report);
    Require(report.lines.empty());
    ERR_print_errors_cb_nid_postfix(nullptr, nullptr);

    report.enqueueOnFirst = true;
    LibreSslPutError_nid_no_patch(3, 100, 65, "crypto.c", 37);
    LibreSslPutError_nid_no_patch(4, 101, 66, nullptr, 88);
    ERR_print_errors_cb_nid_postfix(Capture_nid_no_patch, &report);
    Require(report.lines == std::vector<std::string>{
        prefix + "error:03064041:lib(3):func(100):reason(65):crypto.c:37:\n",
        prefix + "error:04065042:lib(4):func(101):reason(66):NA:0:\n",
        prefix + "error:14066043:lib(20):func(102):reason(67):nested.c:14:\n"});
    Require(ERR_get_error_nid_postfix() == 0 && ERR_peek_last_error_nid_postfix() == 0);
}

void CheckStop(const std::string& prefix) {
    for (const int result : {0, -7}) {
        Report report;
        report.result = result;
        LibreSslPutError_nid_no_patch(3, 100, 65, "stop.c", 9);
        LibreSslPutError_nid_no_patch(4, 101, 66);
        ERR_print_errors_cb_nid_postfix(Capture_nid_no_patch, &report);
        Require(report.lines == std::vector<std::string>{
            prefix + "error:03064041:lib(3):func(100):reason(65):stop.c:9:\n"});
        Require(ERR_get_error_nid_postfix() == 0x04065042 && ERR_get_error_nid_postfix() == 0);
    }
}

void CheckGuestProducer(const std::string& prefix) {
    const unsigned char octetString[]{0x04, 0x01, 0x00};
    const unsigned char* input = octetString;
    Require(d2i_ASN1_INTEGER_nid_postfix(nullptr, &input, sizeof(octetString)) == nullptr);
    Report report;
    ERR_print_errors_cb_nid_postfix(Capture_nid_no_patch, &report);
    Require(report.lines == std::vector<std::string>{
        prefix + "error:0DFFF0A8:lib(13):func(4095):reason(168):NA:0:\n",
        prefix + "error:0DFFF03A:lib(13):func(4095):reason(58):NA:0:\n"});
    Require(ERR_get_error_nid_postfix() == 0);
}

void CheckTruncation(const std::string& prefix) {
    const std::string filename(5000, 'a');
    Report report;
    LibreSslPutError_nid_no_patch(3, 100, 65, filename.c_str(), 37);
    ERR_print_errors_cb_nid_postfix(Capture_nid_no_patch, &report);
    const auto start = prefix + "error:03064041:lib(3):func(100):reason(65):";
    Require(report.lines == std::vector<std::string>{start + std::string(4095 - start.size(), 'a')});
    Require(ERR_get_error_nid_postfix() == 0);
}

void CheckThreads(const std::string& prefix) {
    LibreSslPutError_nid_no_patch(3, 100, 65);
    std::thread worker([&] {
        const auto workerPrefix = ThreadPrefix();
        Require(workerPrefix != prefix && ERR_peek_error_nid_postfix() == 0);
        LibreSslPutError_nid_no_patch(4, 101, 66, "worker.c", 20);
        Report report;
        ERR_print_errors_cb_nid_postfix(Capture_nid_no_patch, &report);
        Require(report.lines == std::vector<std::string>{
            workerPrefix + "error:04065042:lib(4):func(101):reason(66):worker.c:20:\n"});
        Require(ERR_get_error_nid_postfix() == 0);
    });
    worker.join();
    Require(ERR_get_error_nid_postfix() == 0x03064041);
}

} // namespace

int main() {
    try {
        const auto prefix = ThreadPrefix();
        CheckFifo(prefix);
        CheckStop(prefix);
        CheckGuestProducer(prefix);
        CheckTruncation(prefix);
        CheckThreads(prefix);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("LibreSSL3 error report passed");
    return 0;
}
