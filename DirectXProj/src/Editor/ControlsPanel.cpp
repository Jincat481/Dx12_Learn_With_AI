#include "Core/stdafx.h"
#include "Editor/ControlsPanel.h"
#include "Editor/EditorStyle.h"

void ControlsPanel::Draw(HDC hdc, int viewportWidth, int viewportHeight)
{
    if (!m_visible || !hdc || m_lines.empty())
        return;

    (void)viewportWidth;

    const int panelHeight = kHeaderHeight + kPadding +
                            static_cast<int>(m_lines.size()) * kRowHeight + kPadding;

    const int left = kMargin;
    const int bottom = viewportHeight - kMargin;
    const int top = bottom - panelHeight;
    const int right = left + kPanelWidth;

    RECT panelRect = { left, top, right, bottom };
    editor::FillSolid(hdc, panelRect, editor::kPanelBackground);

    RECT headerRect = { left, top, right, top + kHeaderHeight };
    editor::FillSolid(hdc, headerRect, editor::kHeaderBackground);
    editor::FrameSolid(hdc, panelRect, editor::kPanelBorder);

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, editor::GetUIFontBold()));
    const int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);

    editor::DrawLabel(hdc, left + kPadding, top + 4, m_title, editor::kTextNormal);

    int y = top + kHeaderHeight + kPadding - 4;
    for (const Line& line : m_lines)
    {
        // 키 이름은 굵게, 설명은 보통, 현재 값은 강조색으로.
        ::SelectObject(hdc, editor::GetUIFontBold());
        editor::DrawLabel(hdc, left + kPadding, y, line.key, editor::kTextNormal);

        ::SelectObject(hdc, editor::GetUIFont());
        editor::DrawLabel(hdc, left + kPadding + kKeyColumnWidth, y, line.action, editor::kTextDim);

        if (!line.value.empty())
        {
            const int valueX = left + kPadding + kKeyColumnWidth + 74;
            editor::DrawLabel(hdc, valueX, y, line.value,
                              line.highlight ? editor::kTextSelected : editor::kTextNormal);
        }

        y += kRowHeight;
    }

    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, oldFont);
}
