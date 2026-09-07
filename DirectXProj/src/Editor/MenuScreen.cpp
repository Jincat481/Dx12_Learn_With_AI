#include "Core/stdafx.h"
#include "Editor/MenuScreen.h"
#include "Editor/EditorStyle.h"
#include "Input/InputManager.h"

void MenuScreen::SetEntries(std::vector<Entry> entries)
{
    m_entries = std::move(entries);
    m_focused = 0;
    m_hovered = kNoSelection;
}

void MenuScreen::BuildLayout(int viewportWidth, int viewportHeight)
{
    m_rects.clear();
    if (m_entries.empty())
        return;

    const int count = static_cast<int>(m_entries.size());
    const int listHeight = count * kEntryHeight + (count - 1) * kEntryGap;
    const int panelHeight = kHeaderHeight + listHeight + kPadding * 2;

    const int left = (viewportWidth - kPanelWidth) / 2;
    const int top = (viewportHeight - panelHeight) / 2;

    m_panelRect = { left, top, left + kPanelWidth, top + panelHeight };

    int y = top + kHeaderHeight + kPadding;
    for (int i = 0; i < count; ++i)
    {
        m_rects.push_back({ left + kPadding, y, left + kPanelWidth - kPadding, y + kEntryHeight });
        y += kEntryHeight + kEntryGap;
    }
}

int MenuScreen::Update(const InputManager& input, int viewportWidth, int viewportHeight)
{
    BuildLayout(viewportWidth, viewportHeight);

    if (m_entries.empty())
        return kNoSelection;

    const int count = static_cast<int>(m_entries.size());

    // ---- 마우스 ----
    const int mouseX = input.GetMouseX();
    const int mouseY = input.GetMouseY();

    m_hovered = kNoSelection;
    for (int i = 0; i < count; ++i)
    {
        const RECT& rect = m_rects[i];
        if (mouseX >= rect.left && mouseX < rect.right && mouseY >= rect.top && mouseY < rect.bottom)
        {
            m_hovered = i;
            m_focused = i;      // 마우스를 올리면 키보드 선택도 따라간다
            break;
        }
    }

    if (m_hovered != kNoSelection && input.GetMouseButtonDown(InputManager::Left))
        return m_hovered;

    // ---- 키보드 ----
    if (input.GetKeyDown(VK_DOWN)) m_focused = (m_focused + 1) % count;
    if (input.GetKeyDown(VK_UP))   m_focused = (m_focused - 1 + count) % count;

    if (input.GetKeyDown(VK_RETURN) || input.GetKeyDown(VK_SPACE))
        return m_focused;

    return kNoSelection;
}

void MenuScreen::Draw(HDC hdc, int viewportWidth, int viewportHeight)
{
    if (!hdc || m_entries.empty())
        return;

    BuildLayout(viewportWidth, viewportHeight);

    // ---- 패널 ----
    editor::FillSolid(hdc, m_panelRect, editor::kPanelBackground);

    RECT header = { m_panelRect.left, m_panelRect.top, m_panelRect.right, m_panelRect.top + kHeaderHeight };
    editor::FillSolid(hdc, header, editor::kHeaderBackground);
    editor::FrameSolid(hdc, m_panelRect, editor::kPanelBorder);

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, editor::GetUIFontBold()));
    const int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);

    editor::DrawLabel(hdc, m_panelRect.left + kPadding, m_panelRect.top + 26,
                      L"DirectXProj 쇼케이스", editor::kTextSelected);

    ::SelectObject(hdc, editor::GetUIFont());
    editor::DrawLabel(hdc, m_panelRect.left + kPadding, m_panelRect.top + 52,
                      L"보고 싶은 기능을 고른다.  ↑ ↓ 이동 · Enter 선택 · 클릭도 된다",
                      editor::kTextDim);

    // ---- 항목 ----
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const RECT& rect = m_rects[i];
        const bool focused = (static_cast<int>(i) == m_focused);

        editor::FillSolid(hdc, rect, focused ? editor::kSelectedRow : editor::kFieldBackground);
        editor::FrameSolid(hdc, rect, editor::kFieldBorder);

        ::SelectObject(hdc, editor::GetUIFontBold());
        editor::DrawLabel(hdc, rect.left + 16, rect.top + 12, m_entries[i].title,
                          focused ? editor::kTextSelected : editor::kTextNormal);

        ::SelectObject(hdc, editor::GetUIFont());
        editor::DrawLabel(hdc, rect.left + 16, rect.top + 34, m_entries[i].description,
                          focused ? editor::kTextSelected : editor::kTextDim);
    }

    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, oldFont);
}
