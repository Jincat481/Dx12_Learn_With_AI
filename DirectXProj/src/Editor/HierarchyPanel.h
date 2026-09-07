#pragma once
#include "Core/stdafx.h"

class Graphics;
class Scene;
class GameObject;
class InputManager;

// =============================================================
// HierarchyPanel
//  Unity 의 Hierarchy 창처럼 Scene 의 부모-자식 구조를 트리로 보여준다.
//
//  - 루트부터 자식으로 내려가며 들여쓰기해서 계층을 드러낸다
//  - 자식이 있는 항목은 ▶ / ▼ 로 접고 편다
//  - 선택된 오브젝트는 파란 줄로 강조된다(피킹 선택과 동기화)
//  - 줄을 다른 줄 위로 끌어다 놓으면 그 오브젝트의 자식이 된다
//  - 목록 아래 빈 곳에 놓으면 부모에서 떨어져 루트가 된다
//
//  그리기는 Graphics 의 GDI 오버레이를 쓴다. 레이아웃은 Update 에서 계산하므로
//  같은 프레임에 입력 판정과 그리기가 일치한다.
// =============================================================
class HierarchyPanel
{
public:
    // Update 가 돌려주는 요청. Game 이 실제 Scene 변경을 수행한다.
    struct Action
    {
        uint64_t selectId = 0;        // 새로 선택할 GameObject (0 = 변경 없음)
        bool     reparent = false;    // 아래 두 값이 유효한지
        uint64_t dragId = 0;          // 옮길 대상
        uint64_t newParentId = 0;     // 새 부모 (0 = 루트로 분리)
    };

    HierarchyPanel() = default;

    // 마우스/키 입력을 처리하고 레이아웃을 갱신한다.
    Action Update(Scene& scene, const InputManager& input, uint64_t selectedId);

    // 씬 Render 이후, Present 이전에 호출한다. HDC 는 Graphics::BeginOverlay 로 얻는다.
    void Draw(HDC hdc, uint64_t selectedId);

    bool Contains(int x, int y) const;
    bool IsVisible() const { return m_visible; }
    void Toggle() { m_visible = !m_visible; CancelDrag(); }

private:
    struct Row
    {
        uint64_t     id = 0;
        int          depth = 0;
        bool         hasChildren = false;
        bool         collapsed = false;
        bool         active = true;
        std::wstring label;
        RECT         rect{};
        RECT         arrow{};
    };

    void BuildRows(Scene& scene);
    void AppendObject(GameObject* object, int depth);

    bool IsCollapsed(uint64_t id) const;
    void ToggleCollapsed(uint64_t id);
    void CancelDrag();

    const Row* FindRowAt(int x, int y) const;

    std::vector<Row>      m_rows;
    std::vector<uint64_t> m_collapsed;

    bool m_visible = true;

    // 드래그 상태
    bool     m_dragging = false;
    uint64_t m_dragId = 0;
    int      m_pressX = 0;
    int      m_pressY = 0;
    uint64_t m_dropTargetId = 0;    // 지금 커서가 올라가 있는 줄
    bool     m_dropToRoot = false;  // 빈 곳(루트로 분리) 위인지

    // 레이아웃 (클라이언트 좌표)
    static constexpr int kPanelX = 12;
    static constexpr int kPanelY = 12;
    static constexpr int kPanelWidth = 280;
    static constexpr int kRowHeight = 20;
    static constexpr int kHeaderHeight = 26;
    static constexpr int kIndentWidth = 16;
    static constexpr int kPadding = 6;
    static constexpr int kDropZoneHeight = 22;   // 목록 아래 "루트로 분리" 영역
    static constexpr int kDragThreshold = 4;

    int m_panelHeight = kHeaderHeight + kPadding * 2 + kDropZoneHeight;
    int m_listBottom = 0;
};
