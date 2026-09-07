#pragma once
#include "Core/stdafx.h"

class InputManager;

// =============================================================
// MenuScreen
//  구현한 기능들을 골라서 들어가는 시작 메뉴.
//  Hierarchy / Inspector 와 같은 GDI 오버레이로 그린다.
//
//  마우스로 항목을 누르거나, 위/아래 키로 옮기고 Enter 로 선택한다.
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

    std::vector<Entry> m_entries;
    std::vector<RECT>  m_rects;

    int m_hovered = kNoSelection;
    int m_focused = 0;              // 키보드 선택 위치

    RECT m_panelRect{};

    static constexpr int kPanelWidth = 520;
    static constexpr int kEntryHeight = 62;
    static constexpr int kEntryGap = 10;
    static constexpr int kHeaderHeight = 96;
    static constexpr int kPadding = 22;
};
