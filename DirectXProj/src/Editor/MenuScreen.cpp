#include "Core/stdafx.h"
#include "Editor/MenuScreen.h"
#include "Editor/EditorStyle.h"
#include "Input/InputManager.h"

#include <cstdio>

void MenuScreen::SetEntries(std::vector<Entry> entries)
{
    m_entries = std::move(entries);
    m_focused = 0;
    m_scroll = 0;
    m_hovered = kNoSelection;
}

// -------------------------------------------------------------
// 배치
//  화면 높이에서 머리말/꼬리말을 뺀 만큼만 목록에 쓴다.
//  들어가는 개수(maxVisible)를 먼저 정하고 그만큼만 사각형을 만든다.
//  나머지 항목은 스크롤해서 본다.
// -------------------------------------------------------------
void MenuScreen::BuildLayout(int viewportWidth, int viewportHeight)
{
    m_rects.clear();

    const int count = static_cast<int>(m_entries.size());
    if (count == 0)
        return;

    const int step = kEntryHeight + kEntryGap;

    const int availableForList =
        viewportHeight - kMargin * 2 - kHeaderHeight - kFooterHeight - kPadding * 2;

    m_maxVisible = (std::max)(1, (availableForList + kEntryGap) / step);
    m_maxVisible = (std::min)(m_maxVisible, count);

    ClampScroll();

    const int listHeight = m_maxVisible * kEntryHeight + (m_maxVisible - 1) * kEntryGap;
    const int panelHeight = kHeaderHeight + kPadding + listHeight + kPadding + kFooterHeight;

    const int left = (viewportWidth - kPanelWidth) / 2;
    const int top = (std::max)(kMargin, (viewportHeight - panelHeight) / 2);

    m_panelRect = { left, top, left + kPanelWidth, top + panelHeight };

    const int listTop = top + kHeaderHeight + kPadding;
    m_listRect = { left + kPadding, listTop, left + kPanelWidth - kPadding, listTop + listHeight };

    int y = listTop;
    for (int i = 0; i < m_maxVisible; ++i)
    {
        m_rects.push_back({ left + kPadding, y,
                            left + kPanelWidth - kPadding - kScrollBarWidth - 4, y + kEntryHeight });
        y += step;
    }
}

void MenuScreen::ClampScroll()
{
    const int count = static_cast<int>(m_entries.size());
    const int maxScroll = (std::max)(0, count - m_maxVisible);

    m_scroll = (std::max)(0, (std::min)(maxScroll, m_scroll));
}

// 선택한 항목이 화면 밖이면 보이도록 스크롤을 옮긴다.
void MenuScreen::EnsureFocusVisible()
{
    if (m_focused < m_scroll)
        m_scroll = m_focused;
    else if (m_focused >= m_scroll + m_maxVisible)
        m_scroll = m_focused - m_maxVisible + 1;

    ClampScroll();
}

int MenuScreen::Update(const InputManager& input, int viewportWidth, int viewportHeight)
{
    BuildLayout(viewportWidth, viewportHeight);

    const int count = static_cast<int>(m_entries.size());
    if (count == 0)
        return kNoSelection;

    // ---- 휠 스크롤 ----
    if (const int wheel = input.GetMouseWheelDelta(); wheel != 0)
    {
        m_scroll -= wheel / WHEEL_DELTA;   // 위로 굴리면 목록도 위로
        ClampScroll();
        BuildLayout(viewportWidth, viewportHeight);
    }

    // ---- 마우스 ----
    const int mouseX = input.GetMouseX();
    const int mouseY = input.GetMouseY();

    m_hovered = kNoSelection;
    for (int i = 0; i < static_cast<int>(m_rects.size()); ++i)
    {
        const RECT& rect = m_rects[i];
        if (mouseX >= rect.left && mouseX < rect.right && mouseY >= rect.top && mouseY < rect.bottom)
        {
            m_hovered = m_scroll + i;
            m_focused = m_hovered;
            break;
        }
    }

    if (m_hovered != kNoSelection && input.GetMouseButtonDown(InputManager::Left))
        return m_hovered;

    // ---- 키보드 ----
    bool moved = false;
    if (input.GetKeyDown(VK_DOWN))  { m_focused = (m_focused + 1) % count; moved = true; }
    if (input.GetKeyDown(VK_UP))    { m_focused = (m_focused - 1 + count) % count; moved = true; }
    if (input.GetKeyDown(VK_NEXT))  { m_focused = (std::min)(count - 1, m_focused + m_maxVisible); moved = true; }
    if (input.GetKeyDown(VK_PRIOR)) { m_focused = (std::max)(0, m_focused - m_maxVisible); moved = true; }
    if (input.GetKeyDown(VK_HOME))  { m_focused = 0; moved = true; }
    if (input.GetKeyDown(VK_END))   { m_focused = count - 1; moved = true; }

    if (moved)
    {
        EnsureFocusVisible();
        BuildLayout(viewportWidth, viewportHeight);
    }

    if (input.GetKeyDown(VK_RETURN) || input.GetKeyDown(VK_SPACE))
        return m_focused;

    return kNoSelection;
}

void MenuScreen::Draw(HDC hdc, int viewportWidth, int viewportHeight)
{
    if (!hdc || m_entries.empty())
        return;

    BuildLayout(viewportWidth, viewportHeight);

    const int count = static_cast<int>(m_entries.size());

    // ---- 패널 ----
    editor::FillSolid(hdc, m_panelRect, editor::kPanelBackground);

    RECT header = { m_panelRect.left, m_panelRect.top, m_panelRect.right, m_panelRect.top + kHeaderHeight };
    editor::FillSolid(hdc, header, editor::kHeaderBackground);
    editor::FrameSolid(hdc, m_panelRect, editor::kPanelBorder);

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, editor::GetUIFontBold()));
    const int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);

    editor::DrawLabel(hdc, m_panelRect.left + kPadding, m_panelRect.top + 16,
                      L"DirectXProj 쇼케이스", editor::kTextSelected);

    ::SelectObject(hdc, editor::GetUIFont());
    editor::DrawLabel(hdc, m_panelRect.left + kPadding, m_panelRect.top + 42,
                      L"↑ ↓ 이동 · Enter 선택 · 휠 스크롤 · 클릭",
                      editor::kTextDim);

    // ---- 항목 ----
    for (int i = 0; i < static_cast<int>(m_rects.size()); ++i)
    {
        const int index = m_scroll + i;
        if (index >= count)
            break;

        const RECT& rect = m_rects[i];
        const bool focused = (index == m_focused);

        editor::FillSolid(hdc, rect, focused ? editor::kSelectedRow : editor::kFieldBackground);
        editor::FrameSolid(hdc, rect, editor::kFieldBorder);

        ::SelectObject(hdc, editor::GetUIFontBold());
        editor::DrawLabel(hdc, rect.left + 16, rect.top + 9, m_entries[index].title,
                          focused ? editor::kTextSelected : editor::kTextNormal);

        ::SelectObject(hdc, editor::GetUIFont());
        editor::DrawLabel(hdc, rect.left + 16, rect.top + 30, m_entries[index].description,
                          focused ? editor::kTextSelected : editor::kTextDim);
    }

    // ---- 스크롤 막대와 위치 안내 ----
    ::SelectObject(hdc, editor::GetUIFont());

    if (count > m_maxVisible)
    {
        const int trackLeft = m_panelRect.right - kPadding - kScrollBarWidth;
        RECT track = { trackLeft, m_listRect.top, trackLeft + kScrollBarWidth, m_listRect.bottom };
        editor::FillSolid(hdc, track, editor::kFieldBackground);

        const int trackHeight = track.bottom - track.top;
        const int thumbHeight = (std::max)(24, trackHeight * m_maxVisible / count);
        const int maxScroll = count - m_maxVisible;
        const int thumbTop = track.top +
                             (maxScroll > 0 ? (trackHeight - thumbHeight) * m_scroll / maxScroll : 0);

        RECT thumb = { track.left, thumbTop, track.right, thumbTop + thumbHeight };
        editor::FillSolid(hdc, thumb, editor::kSelectedRow);

    }

    wchar_t footer[48];
    _snwprintf_s(footer, _countof(footer), _TRUNCATE, L"%d / %d", m_focused + 1, count);

    editor::DrawLabel(hdc, m_panelRect.left + kPadding, m_listRect.bottom + kPadding - 4,
                      footer, editor::kTextDim);

    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, oldFont);
}
