#pragma once
#include <string>

// 저장 파일은 UTF-8, Win32 API 는 UTF-16(wide) 이므로 변환 지점을 한 곳에 모은다. (S20)
namespace StringUtil
{
    std::string  WideToUtf8(const std::wstring& text);
    std::wstring Utf8ToWide(const std::string& text);
}
