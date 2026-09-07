#pragma once
#include "Core/stdafx.h"

// =============================================================
// 에디터 UI 공통 스타일 / GDI 헬퍼
//  Hierarchy 와 Inspector 가 같은 색과 폰트를 쓰도록 한곳에 모았다.
// =============================================================
namespace editor
{
    // Unity 다크 테마와 비슷한 색
    constexpr COLORREF kPanelBackground  = RGB(56, 56, 56);
    constexpr COLORREF kPanelBorder      = RGB(24, 24, 24);
    constexpr COLORREF kHeaderBackground = RGB(40, 40, 40);
    constexpr COLORREF kSelectedRow      = RGB(44, 93, 135);
    constexpr COLORREF kDropTargetRow    = RGB(70, 120, 70);
    constexpr COLORREF kFieldBackground  = RGB(42, 42, 42);
    constexpr COLORREF kFieldEditing     = RGB(30, 55, 80);
    constexpr COLORREF kFieldBorder      = RGB(28, 28, 28);
    constexpr COLORREF kTextNormal       = RGB(215, 215, 215);
    constexpr COLORREF kTextSelected     = RGB(255, 255, 255);
    constexpr COLORREF kTextDim          = RGB(140, 140, 140);
    constexpr COLORREF kTextInactive     = RGB(120, 120, 120);
    constexpr COLORREF kSeparator        = RGB(30, 30, 30);

    // 프로세스 하나에 폰트 하나. 종료 시 ReleaseUIFont 로 정리한다.
    HFONT GetUIFont();
    HFONT GetUIFontBold();
    void  ReleaseUIFont();

    void FillSolid(HDC hdc, const RECT& rect, COLORREF color);
    void FrameSolid(HDC hdc, const RECT& rect, COLORREF color);
    void DrawLabel(HDC hdc, int x, int y, const std::wstring& text, COLORREF color);

    // 실수를 인스펙터 표기용 문자열로. (예: 12.35)
    std::wstring FormatFloat(float value);

    // 문자열을 실수로. 실패하면 false.
    bool ParseFloat(const std::wstring& text, float& out);
}
