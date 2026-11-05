#pragma once

#include <Windows.h>
#include <Shellapi.h>
#include <cassert>
#include <initializer_list>
#include <string>

// 引号前的反斜杠翻倍；参数末尾的反斜杠也必须保护结束引号。
inline std::wstring QuoteWindowsArgument(const wchar_t* argument) {
    std::wstring quoted = L"\"";
    size_t slashes = 0;
    for (; *argument; ++argument) {
        if (*argument == L'\\') {
            ++slashes;
            continue;
        }
        quoted.append(*argument == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        quoted += *argument;
        slashes = 0;
    }
    quoted.append(slashes * 2, L'\\');
    quoted += L'"';
    return quoted;
}

inline std::wstring BuildWindowsCommandLine(std::initializer_list<const wchar_t*> arguments) {
    std::wstring command;
    for (const wchar_t* argument : arguments) {
        if (!command.empty()) command += L' ';
        command += QuoteWindowsArgument(argument);
    }
    return command;
}

inline void CheckWindowsCommandLineQuoting() {
    const std::initializer_list<const wchar_t*> arguments = {
        L"probe.exe", L"", L"plain", L"two words", L"\t", L"say \"hello\"",
        L"\\", L"E:\\", L"space path\\", L"\\\\server\\share name\\",
        L"slash\\\"quote", L"slashes\\\\\"end\\", L"--child"
    };
    const std::wstring command = BuildWindowsCommandLine(arguments);
    int count = 0;
    wchar_t** parsed = CommandLineToArgvW(command.c_str(), &count);
    assert(parsed != nullptr);
    assert(count == static_cast<int>(arguments.size()));
    int index = 0;
    for (const wchar_t* argument : arguments) {
        assert(lstrcmpW(parsed[index], argument) == 0);
        ++index;
    }
    LocalFree(parsed);
}
