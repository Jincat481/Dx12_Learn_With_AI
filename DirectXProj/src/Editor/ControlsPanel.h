#pragma once
#include "Core/stdafx.h"

// =============================================================
// ControlsPanel
//  "무슨 키를 누르면 무엇이 바뀌는지" 를 화면 왼쪽 아래에 보여준다.
//  현재 상태(예: 지금 표시 모드)를 함께 띄워서, 눌러 보지 않아도
//  무엇이 바뀔지 알 수 있게 한다.
// =============================================================
class ControlsPanel
{
public:
    struct Line
    {
        std::wstring key;        // 예 : "Tab"
        std::wstring action;     // 예 : "표시 모드"
        std::wstring value;      // 예 : "텍스처 스플래팅" (없으면 비워 둔다)
        bool highlight = false;  // 현재 상태를 강조할지
    };

    void SetTitle(std::wstring title) { m_title = std::move(title); }
    void SetLines(std::vector<Line> lines) { m_lines = std::move(lines); }

    void Draw(HDC hdc, int viewportWidth, int viewportHeight);

    bool IsVisible() const { return m_visible; }
    void Toggle() { m_visible = !m_visible; }

private:
    std::wstring      m_title;
    std::vector<Line> m_lines;
    bool m_visible = true;

    static constexpr int kPanelWidth = 330;
    static constexpr int kMargin = 12;
    static constexpr int kPadding = 10;
    static constexpr int kHeaderHeight = 24;
    static constexpr int kRowHeight = 19;
    static constexpr int kKeyColumnWidth = 96;
};
