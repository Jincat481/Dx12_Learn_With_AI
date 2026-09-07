#include "Core/stdafx.h"
#include "Utils/StringUtil.h"

namespace StringUtil
{
    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty())
            return std::string();

        const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                               nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(size), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                              result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
            return std::wstring();

        const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(static_cast<size_t>(size), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }
}
