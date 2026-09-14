#include "Core/stdafx.h"
#include "Editor/ControlsPanel.h"
#include "Editor/EditorStyle.h"

namespace
{
    int MeasureTextWidth(HDC hdc, const std::wstring& text)
    {
        if (text.empty())
            return 0;

        SIZE size{};
        ::GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &size);
        return size.cx;
    }
}

void ControlsPanel::Draw(HDC hdc, int viewportWidth, int viewportHeight)
{
    if (!m_visible || !hdc || m_lines.empty())
        return;

    (void)viewportWidth;

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, editor::GetUIFont()));
    const int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);

    // ---- 열 너비 ----
    //  예전에는 값 열을 고정 위치에 두어 "백그라운드 생성" 같은 긴 설명이 값과 겹쳤다.
    //  글꼴마다 폭이 다르므로 실제 픽셀 폭을 재서 가장 긴 것에 맞춘다.
    int actionWidth = 0;
    int valueWidth = 0;
    for (const Line& line : m_lines)
    {
        actionWidth = (std::max)(actionWidth, MeasureTextWidth(hdc, line.action));
        valueWidth = (std::max)(valueWidth, MeasureTextWidth(hdc, line.value));
    }

    ::SelectObject(hdc, editor::GetUIFontBold());
    int keyWidth = kKeyColumnWidth;
    for (const Line& line : m_lines)
        keyWidth = (std::max)(keyWidth, MeasureTextWidth(hdc, line.key) + kColumnGap);
    const int titleWidth = MeasureTextWidth(hdc, m_title);

    const int actionOffset = keyWidth;
    const int valueOffset = keyWidth + actionWidth + kColumnGap;
    const int contentWidth = (std::max)(valueOffset + valueWidth, titleWidth);
    const int panelWidth = (std::max)(kPanelWidth, contentWidth + kPadding * 2);

    // ---- 패널 ----
    const int panelHeight = kHeaderHeight + kPadding +
                            static_cast<int>(m_lines.size()) * kRowHeight + kPadding;

    const int left = kMargin;
    const int bottom = viewportHeight - kMargin;
    const int top = bottom - panelHeight;
    const int right = left + panelWidth;

    RECT panelRect = { left, top, right, bottom };
    editor::FillSolid(hdc, panelRect, editor::kPanelBackground);

    RECT headerRect = { left, top, right, top + kHeaderHeight };
    editor::FillSolid(hdc, headerRect, editor::kHeaderBackground);
    editor::FrameSolid(hdc, panelRect, editor::kPanelBorder);

    editor::DrawLabel(hdc, left + kPadding, top + 4, m_title, editor::kTextNormal);

    int y = top + kHeaderHeight + kPadding - 4;
    for (const Line& line : m_lines)
    {
        // 키 이름은 굵게, 설명은 보통, 현재 값은 강조색으로.
        ::SelectObject(hdc, editor::GetUIFontBold());
        editor::DrawLabel(hdc, left + kPadding, y, line.key, editor::kTextNormal);

        ::SelectObject(hdc, editor::GetUIFont());
        editor::DrawLabel(hdc, left + kPadding + actionOffset, y, line.action, editor::kTextDim);

        if (!line.value.empty())
        {
            editor::DrawLabel(hdc, left + kPadding + valueOffset, y, line.value,
                              line.highlight ? editor::kTextSelected : editor::kTextNormal);
        }

        y += kRowHeight;
    }

    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, oldFont);
}
