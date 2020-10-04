// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <codecvt>
#include <cstdlib>
#include <locale>
#include <sstream>
#include "common/logging/log.h"
#include "common/string_util.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Common {

/// Make a string lowercase
std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

// Turns "  hej " into "hej". Also handles tabs.
std::string StripSpaces(const std::string& str) {
    const std::size_t s = str.find_first_not_of(" \t\r\n");

    if (str.npos != s) {
        return str.substr(s, str.find_last_not_of(" \t\r\n") - s + 1);
    } else {
        return "";
    }
}

bool SplitPath(const std::string& full_path, std::string* path, std::string* filename,
               std::string* extension) {
    if (full_path.empty()) {
        return false;
    }

    std::size_t dir_end = full_path.find_last_of("/"
// Windows needs the : included for something like just "C:" to be considered a directory
#ifdef _WIN32
                                                 ":"
#endif
    );
    if (std::string::npos == dir_end) {
        dir_end = 0;
    } else {
        dir_end += 1;
    }

    std::size_t filename_end = full_path.rfind('.');
    if (filename_end < dir_end || std::string::npos == filename_end) {
        filename_end = full_path.size();
    }

    if (path) {
        *path = full_path.substr(0, dir_end);
    }

    if (filename) {
        *filename = full_path.substr(dir_end, filename_end - dir_end);
    }

    if (extension) {
        *extension = full_path.substr(filename_end);
    }

    return true;
}

void SplitString(const std::string& str, const char delim, std::vector<std::string>& output) {
    std::istringstream iss(str);
    output.resize(1);

    while (std::getline(iss, *output.rbegin(), delim)) {
        output.emplace_back();
    }

    output.pop_back();
}

std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest) {
    std::size_t pos = 0;

    if (src == dest)
        return result;

    while ((pos = result.find(src, pos)) != std::string::npos) {
        result.replace(pos, src.size(), dest);
        pos += dest.length();
    }

    return result;
}

std::string UTF16ToUTF8(const std::u16string& input) {
#ifdef _MSC_VER
    // Workaround for missing char16_t/char32_t instantiations in MSVC2017
    std::wstring_convert<std::codecvt_utf8_utf16<__int16>, __int16> convert;
    std::basic_string<__int16> tmp_buffer(input.cbegin(), input.cend());
    return convert.to_bytes(tmp_buffer);
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(input);
#endif
}

std::u16string UTF8ToUTF16(const std::string& input) {
#ifdef _MSC_VER
    // Workaround for missing char16_t/char32_t instantiations in MSVC2017
    std::wstring_convert<std::codecvt_utf8_utf16<__int16>, __int16> convert;
    auto tmp_buffer = convert.from_bytes(input);
    return std::u16string(tmp_buffer.cbegin(), tmp_buffer.cend());
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.from_bytes(input);
#endif
}

#ifdef _WIN32
static std::wstring CPToUTF16(u32 code_page, const std::string& input) {
    const auto size =
        MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);

    if (size == 0) {
        return L"";
    }

    std::wstring output(size, L'\0');

    if (size != MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()),
                                    &output[0], static_cast<int>(output.size()))) {
        output.clear();
    }

    return output;
}

std::string UTF16ToUTF8(const std::wstring& input) {
    const auto size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return "";
    }

    std::string output(size, '\0');

    if (size != WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                    &output[0], static_cast<int>(output.size()), nullptr,
                                    nullptr)) {
        output.clear();
    }

    return output;
}

std::wstring UTF8ToUTF16W(const std::string& input) {
    return CPToUTF16(CP_UTF8, input);
}

#endif

std::string StringFromFixedZeroTerminatedBuffer(const char* buffer, std::size_t max_len) {
    std::size_t len = 0;
    while (len < max_len && buffer[len] != '\0')
        ++len;

    return std::string(buffer, len);
}

} // namespace Common
