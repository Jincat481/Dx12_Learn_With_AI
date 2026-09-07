#include "Core/stdafx.h"
#include <cstdio>
#include <cstdarg>

namespace dxutil
{
    void DebugLog(const wchar_t* format, ...)
    {
        wchar_t buffer[2048];
        va_list args;
        va_start(args, format);
        _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
        va_end(args);

        OutputDebugStringW(buffer);
        OutputDebugStringW(L"\n");
        fputws(buffer, stdout);
        fputws(L"\n", stdout);
    }

    bool CheckHR(HRESULT hr, const wchar_t* what, const wchar_t* file, int line)
    {
        if (SUCCEEDED(hr))
            return true;

        DebugLog(L"[HRESULT 실패] %s (0x%08X)\n    at %s:%d", what, static_cast<unsigned>(hr), file, line);
        return false;
    }
}
