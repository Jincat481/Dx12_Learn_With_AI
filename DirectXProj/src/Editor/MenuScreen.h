#pragma once
#include "Core/stdafx.h"

class InputManager;

// =============================================================
// MenuScreen
//  구현한 기능들을 골라서 들어가는 시작 메뉴.
//  Hierarchy / Inspector 와 같은 GDI 오버레이로 그린다.
//
//  항목이 늘어나도 화면을 넘치지 않도록 **스크롤**한다.
//   - 화면에 들어가는 만큼만 그리고, 나머지는 굴려서 본다
//   - 휠로 굴리거나, ↑↓ 로 옮기면 선택 항목이 보이도록 따라 스크롤한다
//   - 오른쪽 스크롤 막대로 지금 어디쯤인지 알려 준다
// =============================================================
class MenuScreen
{
public:
    struct Entry
    {
        std::wstring title;
        std::wstring description;
    };

    static constexpr int kNoSelection = -1;

    void SetEntries(std::vector<Entry> entries);

    // 고른 항목의 인덱스를 돌려준다. 고르지 않았으면 kNoSelection.
    int Update(const InputManager& input, int viewportWidth, int viewportHeight);

    void Draw(HDC hdc, int viewportWidth, int viewportHeight);

private:
    void BuildLayout(int viewportWidth, int viewportHeight);
    void ClampScroll();
    void EnsureFocusVisible();

    std::vector<Entry> m_entries;

    // 지금 화면에 보이는 항목들의 사각형. m_scroll 번째부터 차례로 대응한다.
    std::vector<RECT>  m_rects;
    int m_maxVisible = 1;

    int m_hovered = kNoSelection;
    int m_focused = 0;
    int m_scroll = 0;          // 맨 위에 보일 항목 번호

    RECT m_panelRect{};
    RECT m_listRect{};

    static constexpr int kPanelWidth = 560;
    static constexpr int kEntryHeight = 58;
    static constexpr int kEntryGap = 8;
    static constexpr int kHeaderHeight = 78;
    static constexpr int kFooterHeight = 24;
    static constexpr int kPadding = 18;
    static constexpr int kMargin = 16;
    static constexpr int kScrollBarWidth = 6;
};
