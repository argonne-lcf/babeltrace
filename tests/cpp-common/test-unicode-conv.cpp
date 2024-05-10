/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS, Inc.
 */

#include <cstring>

#include "cpp-common/bt2c/call.hpp"
#include "cpp-common/bt2c/unicode-conv.hpp"
#include "cpp-common/vendor/fmt/core.h"

#include "tap/tap.h"

namespace {

constexpr std::uint8_t refUtf8String[] = {
    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64, 0x21, 0x20, 0xc3, 0x85,
    0xc3, 0xa5, 0xc3, 0x89, 0xc3, 0xa9, 0xc3, 0x9c, 0xc3, 0xbc, 0x20, 0xf0, 0x9f, 0x8c, 0x8d, 0xf0,
    0x9f, 0x9a, 0x80, 0x20, 0xd0, 0x9f, 0xd1, 0x80, 0xd0, 0xb8, 0xd0, 0xb2, 0xd0, 0xb5, 0xd1, 0x82,
    0x20, 0xce, 0x93, 0xce, 0xb5, 0xce, 0xb9, 0xce, 0xac, 0x20, 0xcf, 0x83, 0xce, 0xbf, 0xcf, 0x85,
    0x20, 0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd, 0x20, 0xe2, 0x88, 0x91, 0xe2, 0x88, 0x8f, 0x00,
};

constexpr std::uint8_t utf16BeString[] = {
    0x00, 0x48, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x6c, 0x00, 0x6f, 0x00, 0x2c, 0x00, 0x20, 0x00, 0x57,
    0x00, 0x6f, 0x00, 0x72, 0x00, 0x6c, 0x00, 0x64, 0x00, 0x21, 0x00, 0x20, 0x00, 0xc5, 0x00, 0xe5,
    0x00, 0xc9, 0x00, 0xe9, 0x00, 0xdc, 0x00, 0xfc, 0x00, 0x20, 0xd8, 0x3c, 0xdf, 0x0d, 0xd8, 0x3d,
    0xde, 0x80, 0x00, 0x20, 0x04, 0x1f, 0x04, 0x40, 0x04, 0x38, 0x04, 0x32, 0x04, 0x35, 0x04, 0x42,
    0x00, 0x20, 0x03, 0x93, 0x03, 0xb5, 0x03, 0xb9, 0x03, 0xac, 0x00, 0x20, 0x03, 0xc3, 0x03, 0xbf,
    0x03, 0xc5, 0x00, 0x20, 0x4f, 0x60, 0x59, 0x7d, 0x00, 0x20, 0x22, 0x11, 0x22, 0x0f, 0x00, 0x00,
};

constexpr std::uint8_t utf16LeString[] = {
    0x48, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x6c, 0x00, 0x6f, 0x00, 0x2c, 0x00, 0x20, 0x00, 0x57, 0x00,
    0x6f, 0x00, 0x72, 0x00, 0x6c, 0x00, 0x64, 0x00, 0x21, 0x00, 0x20, 0x00, 0xc5, 0x00, 0xe5, 0x00,
    0xc9, 0x00, 0xe9, 0x00, 0xdc, 0x00, 0xfc, 0x00, 0x20, 0x00, 0x3c, 0xd8, 0x0d, 0xdf, 0x3d, 0xd8,
    0x80, 0xde, 0x20, 0x00, 0x1f, 0x04, 0x40, 0x04, 0x38, 0x04, 0x32, 0x04, 0x35, 0x04, 0x42, 0x04,
    0x20, 0x00, 0x93, 0x03, 0xb5, 0x03, 0xb9, 0x03, 0xac, 0x03, 0x20, 0x00, 0xc3, 0x03, 0xbf, 0x03,
    0xc5, 0x03, 0x20, 0x00, 0x60, 0x4f, 0x7d, 0x59, 0x20, 0x00, 0x11, 0x22, 0x0f, 0x22, 0x00, 0x00,
};

constexpr std::uint8_t utf32BeString[] = {
    0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x6c,
    0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x57,
    0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x64,
    0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0xe5,
    0x00, 0x00, 0x00, 0xc9, 0x00, 0x00, 0x00, 0xe9, 0x00, 0x00, 0x00, 0xdc, 0x00, 0x00, 0x00, 0xfc,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x01, 0xf3, 0x0d, 0x00, 0x01, 0xf6, 0x80, 0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x04, 0x1f, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x04, 0x38, 0x00, 0x00, 0x04, 0x32,
    0x00, 0x00, 0x04, 0x35, 0x00, 0x00, 0x04, 0x42, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x03, 0x93,
    0x00, 0x00, 0x03, 0xb5, 0x00, 0x00, 0x03, 0xb9, 0x00, 0x00, 0x03, 0xac, 0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x03, 0xc3, 0x00, 0x00, 0x03, 0xbf, 0x00, 0x00, 0x03, 0xc5, 0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x4f, 0x60, 0x00, 0x00, 0x59, 0x7d, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x22, 0x11,
    0x00, 0x00, 0x22, 0x0f, 0x00, 0x00, 0x00, 0x00,
};

constexpr std::uint8_t utf32LeString[] = {
    0x48, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00,
    0x6f, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x57, 0x00, 0x00, 0x00,
    0x6f, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00,
    0x21, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0xe5, 0x00, 0x00, 0x00,
    0xc9, 0x00, 0x00, 0x00, 0xe9, 0x00, 0x00, 0x00, 0xdc, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x0d, 0xf3, 0x01, 0x00, 0x80, 0xf6, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x1f, 0x04, 0x00, 0x00, 0x40, 0x04, 0x00, 0x00, 0x38, 0x04, 0x00, 0x00, 0x32, 0x04, 0x00, 0x00,
    0x35, 0x04, 0x00, 0x00, 0x42, 0x04, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x93, 0x03, 0x00, 0x00,
    0xb5, 0x03, 0x00, 0x00, 0xb9, 0x03, 0x00, 0x00, 0xac, 0x03, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0xc3, 0x03, 0x00, 0x00, 0xbf, 0x03, 0x00, 0x00, 0xc5, 0x03, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x60, 0x4f, 0x00, 0x00, 0x7d, 0x59, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x11, 0x22, 0x00, 0x00,
    0x0f, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * A UTF-16BE string that abruptly ends in the middle of a code point
 * (but with complete code units).
 */
constexpr std::uint8_t utf16BeTruncCodePoint[] = {
    0x00, 0x43, 0x00, 0x68, 0x00, 0x61, 0x00, 0x74, 0x00, 0x6f, 0x00, 0x6e, 0x00, 0x20, 0xd8, 0x3d,
};

/*
 * A UTF-16BE string that abruptly ends in the middle of a code unit.
 */
constexpr std::uint8_t utf16BeTruncCodeUnit[] = {
    0x00, 0x43, 0x00, 0x68, 0x00, 0x61, 0x00, 0x74, 0x00, 0x6f, 0x00, 0x6e, 0x00,
};

/*
 * A UTF-32BE string that abruptly ends in the middle of a code unit.
 */
constexpr std::uint8_t utf32BeTruncCodeUnit[] = {
    0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x6f,
    0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x64,
    0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x20, 0x00, 0x01, 0xf9,
};

std::string dump(const bt2c::ConstBytes bytes)
{
    std::string res;

    for (const auto byte : bytes) {
        res += fmt::format("{:02x} ", byte);
    }

    return res;
}

/*
 * Checks that `result` matches `refUtf8String` after a conversion from
 * the encoding named `sourceEncoding`.
 */
void checkPass(const bt2c::ConstBytes result, const char * const sourceEncoding)
{
    bool passed = ok(result.size() == sizeof(refUtf8String), "%s to UTF-8: length is expected",
                     sourceEncoding);

    passed &= ok(std::memcmp(result.data(), refUtf8String,
                             std::min(result.size(), sizeof(refUtf8String))) == 0,
                 "%s to UTF-8: content is expected", sourceEncoding);

    if (!passed) {
        diag("Expected: %s\n", dump(refUtf8String).c_str());
        diag("Actual:   %s\n", dump(result).c_str());
    }
}

/*
 * Checks that calling `f()` throws `bt2::Error` and appends a cause
 * having the message `expectedCauseMsg` to the error of the current
 * thread.
 */
template <typename FuncT>
void checkFail(FuncT&& f, const char * const testName, const bt2c::CStringView expectedCauseMsg)
{
    const auto gotError = bt2c::call([&f] {
        try {
            f();
        } catch (const bt2::Error&) {
            return true;
        }

        return false;
    });

    ok(gotError, "%s - got error", testName);

    const auto error = bt_current_thread_take_error();
    const auto msg = bt_error_cause_get_message(bt_error_borrow_cause_by_index(error, 0));

    if (!ok(expectedCauseMsg == msg, "%s - error cause message is expected", testName)) {
        diag("Expecting `%s`", msg);
    }

    bt_error_release(error);
}

} /* namespace */

int main()
{
    plan_tests(14);

    const bt2c::Logger logger {"test-module", "test-tag", bt2c::Logger::Level::None};
    bt2c::UnicodeConv conv {logger};

    checkPass(conv.utf8FromUtf16Be(utf16BeString), "UTF-16BE");
    checkPass(conv.utf8FromUtf16Le(utf16LeString), "UTF-16LE");
    checkPass(conv.utf8FromUtf32Be(utf32BeString), "UTF-32BE");
    checkPass(conv.utf8FromUtf32Le(utf32LeString), "UTF-32LE");

    checkFail(
        [&conv] {
            conv.utf8FromUtf16Be(utf16BeTruncCodePoint);
        },
        "truncated code point",
        "g_iconv() failed: Invalid argument: input-byte-offset=14, from-encoding=UTF-16BE, to-encoding=UTF-8");

    checkFail(
        [&conv] {
            conv.utf8FromUtf16Be(utf16BeTruncCodeUnit);
        },
        "truncated code unit",
        "g_iconv() failed: Invalid argument: input-byte-offset=12, from-encoding=UTF-16BE, to-encoding=UTF-8");

    checkFail(
        [&conv] {
            conv.utf8FromUtf32Be(utf32BeTruncCodeUnit);
        },
        "truncated code unit",
        "g_iconv() failed: Invalid argument: input-byte-offset=32, from-encoding=UTF-32BE, to-encoding=UTF-8");

    return exit_status();
}