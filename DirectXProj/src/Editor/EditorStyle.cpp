#include "Core/stdafx.h"
#include "Editor/EditorStyle.h"

#include <cstdio>
#include <cwchar>
#include <cstdlib>

namespace editor
{
    namespace
    {
        HFONT g_font = nullptr;
        HFONT g_fontBold = nullptr;

        HFONT MakeFont(int weight)
        {
            // 한글 이름도 그대로 나오도록 시스템 UI 폰트를 쓴다.
            return ::CreateFontW(
                -12, 0, 0, 0, weight,
                FALSE, FALSE, FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                L"맑은 고딕");
        }
    }

    HFONT GetUIFont()
    {
        if (!g_font)
            g_font = MakeFont(FW_NORMAL);
        return g_font;
    }

    HFONT GetUIFontBold()
    {
        if (!g_fontBold)
            g_fontBold = MakeFont(FW_BOLD);
        return g_fontBold;
    }

    void ReleaseUIFont()
    {
        if (g_font)     { ::DeleteObject(g_font);     g_font = nullptr; }
        if (g_fontBold) { ::DeleteObject(g_fontBold); g_fontBold = nullptr; }
    }

    void FillSolid(HDC hdc, const RECT& rect, COLORREF color)
    {
        HBRUSH brush = ::CreateSolidBrush(color);
        ::FillRect(hdc, &rect, brush);
        ::DeleteObject(brush);
    }

    void FrameSolid(HDC hdc, const RECT& rect, COLORREF color)
    {
        HBRUSH brush = ::CreateSolidBrush(color);
        ::FrameRect(hdc, &rect, brush);
        ::DeleteObject(brush);
    }

    void DrawLabel(HDC hdc, int x, int y, const std::wstring& text, COLORREF color)
    {
        if (text.empty())
            return;

        ::SetTextColor(hdc, color);
        ::TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
    }

    std::wstring FormatFloat(float value)
    {
        // -0 이 보이지 않게 정리한다.
        if (value == 0.0f)
            value = 0.0f;

        wchar_t buffer[64];
        _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%.2f", value);
        return buffer;
    }

    bool ParseFloat(const std::wstring& text, float& out)
    {
        if (text.empty())
            return false;

        wchar_t* end = nullptr;
        const double value = std::wcstod(text.c_str(), &end);

        if (end == text.c_str())
            return false;                 // 숫자를 하나도 못 읽었다

        while (end && *end == L' ')
            ++end;

        if (end && *end != L'\0')
            return false;                 // 뒤에 이상한 글자가 남았다

        out = static_cast<float>(value);
        return true;
    }
}
